#include "casm/app/ProjectSettings.hh"

#include <tuple>
#include "casm/app/AppIO.hh"
#include "casm/crystallography/Structure.hh"
#include "casm/clex/NeighborList.hh"

#include "casm/clex/ConfigIOSelected.hh"
#include "casm/clex/ConfigSelection.hh"

namespace CASM {

  /// \brief Default weight matrix for approximately spherical neighborhood in Cartesian coordinates
  ///
  /// Equivalent to:
  /// \code
  /// PrimNeighborList::make_weight_matrix(prim.lattice().lat_column_mat(), 10, tol());
  /// \endcode
  Eigen::Matrix3l _default_nlist_weight_matrix(const Structure &prim, double tol) {
    return PrimNeighborList::make_weight_matrix(prim.lattice().lat_column_mat(), 10, tol);
  }

  /// \brief Default includes sublattices with >= 2 components
  std::set<int> _default_nlist_sublat_indices(const Structure &prim) {
    // for now, include sublattices with >= 2 components
    std::set<int> sublat_indices;
    for(int b = 0; b < prim.basis.size(); ++b) {
      if(prim.basis[b].site_occupant().size() >= 2) {
        sublat_indices.insert(b);
      }
    }
    return sublat_indices;
  }

  bool bset_compare(const ClexKey &A, const ClexKey &B) {
    return A.bset < B.bset;
  }

  bool eci_compare(const ClexKey &A, const ClexKey &B) {
    return std::make_tuple(A.name, A.calctype, A.ref, A.bset, A.eci) <
           std::make_tuple(B.name, B.calctype, B.ref, B.bset, B.eci);
  }


  /// \brief Construct CASM project settings for a new project
  ///
  /// \param root Path to new CASM project directory
  /// \param name Name of new CASM project. Use a short title suitable for prepending to file names.
  ///
  ProjectSettings::ProjectSettings(fs::path root, std::string name) :
    m_dir(root),
    m_name(name) {

    if(fs::exists(m_dir.casm_dir())) {
      throw std::runtime_error(
        std::string("Error in 'ProjectSettings(fs::path root, std::string name)'.\n") +
        "  A CASM project already exists at this location: '" + root.string() + "'");
    }

    // check for a prim.json
    if(!fs::is_regular_file(m_dir.prim())) {
      throw std::runtime_error(
        std::string("Error in 'ProjectSettings(fs::path root, std::string name)'.\n") +
        "  No prim.json file found at: " + m_dir.prim().string());
    }

    // generate default nlist settings
    Structure prim(read_prim(m_dir.prim()));
    m_nlist_weight_matrix = _default_nlist_weight_matrix(prim, TOL);
    m_nlist_sublat_indices = _default_nlist_sublat_indices(prim);

    // load ConfigIO
    m_config_io_dict = make_dictionary<Configuration>();

    // default 'selected' uses MASTER
    set_selected(ConfigIO::Selected());
  }

  /// \brief Construct CASM project settings from existing project
  ///
  /// \param root Path to existing CASM project directory. Project settings will be read.
  ///
  ProjectSettings::ProjectSettings(fs::path root) :
    m_dir(root) {

    if(fs::exists(m_dir.casm_dir())) {

      try {

        // read .casmroot current settings
        fs::ifstream file(m_dir.project_settings());
        jsonParser settings(file);

        from_json(m_properties, settings["curr_properties"]);
        from_json(m_clex_key.bset, settings["curr_bset"]);
        from_json(m_clex_key.calctype, settings["curr_calctype"]);
        from_json(m_clex_key.ref, settings["curr_ref"]);
        from_json(m_clex_key.name, settings["curr_clex"]);
        from_json(m_clex_key.eci, settings["curr_eci"]);

        auto _read_if = [&](std::pair<std::string, std::string> &opt, std::string name) {
          if(settings.get_if(opt.first, name)) {
            opt.second = "project_settings";
          }
        };

        auto _read_path_if = [&](std::pair<fs::path, std::string> &opt, std::string name) {
          if(settings.get_if(opt.first, name)) {
            opt.second = "project_settings";
          }
        };

        _read_if(m_cxx, "cxx");
        _read_if(m_cxxflags, "cxxflags");
        _read_if(m_soflags, "soflags");
        _read_path_if(m_casm_prefix, "casm_prefix");
        _read_path_if(m_boost_prefix, "boost_prefix");
        settings.get_if(m_depr_compile_options, "compile_options");
        settings.get_if(m_depr_so_options, "so_options");


        // other options
        settings.get_if(m_view_command, "view_command");
        from_json(m_name, settings["name"]);

        // precision options
        settings.get_else(m_crystallography_tol, "tol", TOL); // deprecated 'tol'
        settings.get_if(m_crystallography_tol, "crystallography_tol");

        settings.get_else(m_lin_alg_tol, "lin_alg_tol", 1e-10);

        // read nlist settings, or generate defaults
        Structure prim;
        bool and_commit = false;
        if(!settings.contains("nlist_weight_matrix") || !settings.contains("nlist_sublat_indices")) {
          _reset_clexulators();
          prim = Structure(read_prim(m_dir.prim()));
          and_commit = true;
        }

        if(settings.contains("nlist_weight_matrix")) {
          from_json(m_nlist_weight_matrix, settings["nlist_weight_matrix"]);
        }
        else {
          m_nlist_weight_matrix = _default_nlist_weight_matrix(prim, crystallography_tol());
        }

        if(settings.contains("nlist_sublat_indices")) {
          from_json(m_nlist_sublat_indices, settings["nlist_sublat_indices"]);
        }
        else {
          m_nlist_sublat_indices = _default_nlist_sublat_indices(prim);
        }

        // load ConfigIO
        m_config_io_dict = make_dictionary<Configuration>();

        // default 'selected' uses MASTER
        set_selected(ConfigIO::Selected());

        // migrate existing query_alias from deprecated 'query_alias.json'
        jsonParser &alias_json = settings["query_alias"];
        if(fs::exists(m_dir.query_alias())) {
          jsonParser depr(m_dir.query_alias());
          for(auto it = depr.begin(); it != depr.end(); ++it) {
            if(!alias_json.contains(it.name())) {
              alias_json[it.name()] = it->get<std::string>();
              and_commit = true;
            }
          }
        }

        // add aliases to dictionary
        for(auto it = alias_json.begin(); it != alias_json.end(); ++it) {
          add_alias(it.name(), it->get<std::string>(), std::cerr);
        }

        if(and_commit) {
          commit();
        }

      }
      catch(std::exception &e) {
        std::cerr << "Error in ProjectSettings::ProjectSettings(const fs::path root).\n" <<
                  "  Error reading " << m_dir.project_settings() << std::endl;
        throw e;
      }
    }
    else if(!fs::exists(m_dir.casm_dir())) {
      throw std::runtime_error(
        std::string("Error in 'ProjectSettings(fs::path root, std::string name)'.\n") +
        "  A CASM project does not exist at this location: '" + root.string() + "'");
    }

  }


  /// \brief Get project name
  std::string ProjectSettings::name() const {
    return m_name;
  }

  /// \brief Get current properties
  const std::vector<std::string> &ProjectSettings::properties() const {
    return m_properties;
  }

  /// \brief Get current basis set name
  std::string ProjectSettings::bset() const {
    return m_clex_key.bset;
  }

  /// \brief Get current calctype name
  std::string ProjectSettings::calctype() const {
    return m_clex_key.calctype;
  }

  /// \brief Get current ref name
  std::string ProjectSettings::ref() const {
    return m_clex_key.ref;
  }

  /// \brief Get current cluster expansion name
  std::string ProjectSettings::clex_name() const {
    return m_clex_key.name;
  }

  /// \brief Get current eci name
  std::string ProjectSettings::eci() const {
    return m_clex_key.eci;
  }

  /// \brief Get current clex key
  ClexKey ProjectSettings::clex_key() const {
    return m_clex_key;
  }


  /// \brief Get neighbor list weight matrix
  Eigen::Matrix3l ProjectSettings::nlist_weight_matrix() const {
    return m_nlist_weight_matrix;
  }

  /// \brief Get set of sublattice indices to include in neighbor lists
  const std::set<int> &ProjectSettings::nlist_sublat_indices() const {
    return m_nlist_sublat_indices;
  }

  /// \brief Get c++ compiler
  std::pair<std::string, std::string> ProjectSettings::cxx() const {
    return m_cxx.first.empty() ? RuntimeLibrary::default_cxx() : m_cxx;
  }

  /// \brief Get c++ compiler options
  std::pair<std::string, std::string> ProjectSettings::cxxflags() const {
    return m_cxxflags.first.empty() ? RuntimeLibrary::default_cxxflags() : m_cxxflags;
  }

  /// \brief Get shared object options
  std::pair<std::string, std::string> ProjectSettings::soflags() const {
    return m_soflags.first.empty() ? RuntimeLibrary::default_soflags() : m_soflags;
  }

  /// \brief Get casm prefix
  std::pair<fs::path, std::string> ProjectSettings::casm_prefix() const {
    return m_casm_prefix.first.empty() ? RuntimeLibrary::default_casm_prefix() : m_casm_prefix;
  }

  /// \brief Get boost prefix
  std::pair<fs::path, std::string> ProjectSettings::boost_prefix() const {
    return m_boost_prefix.first.empty() ? RuntimeLibrary::default_boost_prefix() : m_boost_prefix;
  }

  /// \brief Get current compilation options string
  std::string ProjectSettings::compile_options() const {
    if(!m_depr_compile_options.empty()) {
      return m_depr_compile_options;
    }
    else {
      // else construct from pieces
      return cxx().first + " " + cxxflags().first + " " +
             include_path(casm_prefix().first) + " " +
             include_path(boost_prefix().first);
    }
  }

  /// \brief Get current shared library options string
  std::string ProjectSettings::so_options() const {
    // default to read deprecated 'so_options'
    if(!m_depr_so_options.empty()) {
      return m_depr_so_options;
    }
    else {
      // else construct from pieces
      return cxx().first + " " + soflags().first + " " + link_path(boost_prefix().first);
    }
  }

  /// \brief Get current command used by 'casm view'
  std::string ProjectSettings::view_command() const {
    return m_view_command;
  }

  /// \brief Get current project crystallography tolerance
  double ProjectSettings::crystallography_tol() const {
    return m_crystallography_tol;
  }

  /// \brief Get current project linear algebra tolerance
  double ProjectSettings::lin_alg_tol() const {
    return m_lin_alg_tol;
  }

  // ** Configuration properties **

  const DataFormatterDictionary<Configuration> &ProjectSettings::config_io() const {
    return m_config_io_dict;
  }

  /// \brief Set the selection to be used for the 'selected' column
  void ProjectSettings::set_selected(const ConfigIO::Selected &selection) {
    if(m_config_io_dict.find("selected") != m_config_io_dict.end()) {
      m_config_io_dict.erase("selected");
    }
    m_config_io_dict.insert(
      datum_formatter_alias(
        "selected",
        selection,
        "Returns true if configuration is specified in the input selection"
      )
    );
  }

  /// \brief Set the selection to be used for the 'selected' column
  void ProjectSettings::set_selected(const ConstConfigSelection &selection) {
    set_selected(ConfigIO::selected_in(selection));
  }

  /// \brief Add user-defined query alias
  void ProjectSettings::add_alias(const std::string &alias_name,
                                  const std::string &alias_command,
                                  std::ostream &serr) {

    auto new_formatter =  datum_formatter_alias<Configuration>(alias_name, alias_command, m_config_io_dict);
    auto key = m_config_io_dict.key(new_formatter);

    // if not in dictionary (includes operator dictionary), add
    if(m_config_io_dict.find(key) == m_config_io_dict.end()) {
      m_config_io_dict.insert(new_formatter);
    }
    // if a user-created alias, over-write with message
    else if(m_aliases.find(alias_name) != m_aliases.end()) {
      serr << "WARNING: I already know '" << alias_name << "' as:\n"
           << "             " << m_aliases[alias_name] << "\n"
           << "         I will forget it and learn '" << alias_name << "' as:\n"
           << "             " << alias_command << std::endl;
      m_config_io_dict.insert(new_formatter);
    }
    // else do not add, throw error
    else {
      std::stringstream ss;
      ss << "Error: Attempted to over-write standard CASM query name with user alias.\n";
      throw std::runtime_error(ss.str());
    }

    // save alias
    m_aliases[alias_name] = alias_command;

  }

  /// \brief Return map containing aliases
  ///
  /// - key: alias name
  /// - value: alias command
  const std::map<std::string, std::string> &ProjectSettings::aliases() const {
    return m_aliases;
  }


  // ** Clexulator names **

  std::string ProjectSettings::global_clexulator() const {
    return name() + "_Clexulator";
  }


  // ** Add directories for additional project data **

  /// \brief Create new project data directory
  bool ProjectSettings::new_casm_dir() const {
    return fs::create_directory(m_dir.casm_dir());
  }

  /// \brief Create new symmetry directory
  bool ProjectSettings::new_symmetry_dir() const {
    return fs::create_directory(m_dir.symmetry_dir());
  }

  /// \brief Add a basis set directory
  bool ProjectSettings::new_bset_dir(std::string bset) const {
    return fs::create_directories(m_dir.bset_dir(bset));
  }

  /// \brief Add a cluster expansion directory
  bool ProjectSettings::new_clex_dir(std::string clex) const {
    return fs::create_directories(m_dir.clex_dir(clex));
  }


  /// \brief Add calculation settings directory path
  bool ProjectSettings::new_calc_settings_dir(std::string calctype) const {
    return fs::create_directories(m_dir.calc_settings_dir(calctype));
  }

  /// \brief Add calculation settings directory path, for supercell specific settings
  bool ProjectSettings::new_supercell_calc_settings_dir(std::string scelname, std::string calctype) const {
    return fs::create_directories(m_dir.supercell_calc_settings_dir(scelname, calctype));
  }

  /// \brief Add calculation settings directory path, for configuration specific settings
  bool ProjectSettings::new_configuration_calc_settings_dir(std::string configname, std::string calctype) const {
    return fs::create_directories(m_dir.configuration_calc_settings_dir(configname, calctype));
  }


  /// \brief Add a ref directory
  bool ProjectSettings::new_ref_dir(std::string calctype, std::string ref) const {
    return fs::create_directories(m_dir.ref_dir(calctype, ref));
  }

  /// \brief Add an eci directory
  bool ProjectSettings::new_eci_dir(std::string clex, std::string calctype, std::string ref, std::string bset, std::string eci) const {
    return fs::create_directories(m_dir.eci_dir(clex, calctype, ref, bset, eci));
  }


  // ** Change current settings **

  /// \brief Access current properties
  std::vector<std::string> &ProjectSettings::properties() {
    return m_properties;
  }

  /// \brief Set current basis set to 'bset', if 'bset' exists
  bool ProjectSettings::set_bset(std::string bset) {
    auto all = m_dir.all_bset();
    if(std::find(all.begin(), all.end(), bset) != all.end()) {
      m_clex_key.bset = bset;
      return true;
    }
    return false;
  }

  /// \brief Set current calctype to 'calctype', if 'calctype' exists
  bool ProjectSettings::set_calctype(std::string calctype) {
    auto all = m_dir.all_calctype();
    if(std::find(all.begin(), all.end(), calctype) != all.end()) {
      m_clex_key.calctype = calctype;
      return true;
    }
    return false;
  }

  /// \brief Set current calculation reference to 'ref', if 'ref' exists
  bool ProjectSettings::set_ref(std::string calctype, std::string ref) {
    auto all = m_dir.all_ref(calctype);
    if(std::find(all.begin(), all.end(), ref) != all.end()) {
      m_clex_key.ref = ref;
      return true;
    }
    return false;
  }

  /// \brief Set current cluster expansion name to 'name', if 'name' exists
  bool ProjectSettings::set_clex_name(std::string name) {
    auto all = m_dir.all_clex_name();
    if(std::find(all.begin(), all.end(), name) != all.end()) {
      m_clex_key.name = name;
      return true;
    }
    return false;
  }

  /// \brief Set current eci to 'eci', if 'eci' exists
  bool ProjectSettings::set_eci(std::string clex, std::string calctype, std::string ref, std::string bset, std::string eci) {
    auto all = m_dir.all_eci(clex, calctype, ref, bset);
    if(std::find(all.begin(), all.end(), eci) != all.end()) {
      m_clex_key.eci = eci;
      return true;
    }
    return false;
  }

  /// \brief Set clex settings via clex key
  ///
  /// Sets clex, calctype, ref, bset, eci in turn, aborting if any fail
  bool ProjectSettings::set_clex_key(const ClexKey &key) {
    ClexKey tmp = m_clex_key;
    bool success = set_clex_name(key.name) &&
                   set_calctype(key.calctype) &&
                   set_ref(key.calctype, key.ref) &&
                   set_bset(key.bset) &&
                   set_eci(key.name, key.calctype, key.ref, key.bset, key.eci);
    if(!success) {
      m_clex_key = tmp;
    }
    return success;
  }

  /// \brief Set neighbor list weight matrix (will delete existing Clexulator
  /// source and compiled code)
  bool ProjectSettings::set_nlist_weight_matrix(Eigen::Matrix3l M) {

    // changing the neighbor list properties requires updating Clexulator source code
    // for now we just remove existing source/compiled files
    _reset_clexulators();

    m_nlist_weight_matrix = M;
    return true;
  }


  /// \brief Set c++ compiler (empty string to use default)
  bool ProjectSettings::set_cxx(std::string opt) {
    m_cxx = std::make_pair(opt, "project_settings");
    return true;
  }

  /// \brief Set c++ compiler options (empty string to use default)
  bool ProjectSettings::set_cxxflags(std::string opt)  {
    m_cxxflags = std::make_pair(opt, "project_settings");
    return true;
  }

  /// \brief Set shared object options (empty string to use default)
  bool ProjectSettings::set_soflags(std::string opt)  {
    m_soflags = std::make_pair(opt, "project_settings");
    return true;
  }

  /// \brief Set casm prefix (empty string to use default)
  bool ProjectSettings::set_casm_prefix(fs::path prefix)  {
    m_casm_prefix = std::make_pair(prefix, "project_settings");
    return true;
  }

  /// \brief Set boost prefix (empty string to use default)
  bool ProjectSettings::set_boost_prefix(fs::path prefix)  {
    m_boost_prefix = std::make_pair(prefix, "project_settings");
    return true;
  }


  /// \brief (deprecated) Set compile options to 'opt' (empty string to use default)
  bool ProjectSettings::set_compile_options(std::string opt) {
    m_depr_compile_options = opt;
    return true;
  }

  /// \brief (deprecated) Set shared library options to 'opt' (empty string to use default)
  bool ProjectSettings::set_so_options(std::string opt) {
    m_depr_so_options = opt;
    return true;
  }


  /// \brief Set command used by 'casm view'
  bool ProjectSettings::set_view_command(std::string opt) {
    m_view_command = opt;
    return true;
  }

  /// \brief Set crystallography tolerance
  bool ProjectSettings::set_crystallography_tol(double _tol) {
    m_crystallography_tol = _tol;
    return true;
  }

  /// \brief Set linear algebra tolerance
  bool ProjectSettings::set_lin_alg_tol(double _tol) {
    m_lin_alg_tol = _tol;
    return true;
  }

  /// \brief Save settings to file
  void ProjectSettings::commit() const {

    try {
      SafeOfstream file;
      file.open(m_dir.project_settings());
      jsonParser json;
      to_json(json).print(file.ofstream(), 2, 18);
      file.close();
    }
    catch(...) {
      std::cerr << "Uncaught exception in ProjectSettings::commit()" << std::endl;
      /// re-throw exceptions
      throw;
    }

  }

  /// \brief Changing the neighbor list properties requires updating Clexulator
  /// source code. This will remove existing source/compiled files
  void ProjectSettings::_reset_clexulators() {
    auto all_bset = m_dir.all_bset();
    for(auto it = all_bset.begin(); it != all_bset.end(); ++it) {
      fs::remove(m_dir.clexulator_src(name(), *it));
      fs::remove(m_dir.clexulator_o(name(), *it));
      fs::remove(m_dir.clexulator_so(name(), *it));
    }
  }

  void ProjectSettings::_load_default_options() {
    m_cxx = RuntimeLibrary::default_cxx();
    m_cxxflags = RuntimeLibrary::default_cxxflags();
    m_casm_prefix = RuntimeLibrary::default_casm_prefix();
    m_boost_prefix = RuntimeLibrary::default_boost_prefix();
    m_soflags = RuntimeLibrary::default_soflags();
  }

  jsonParser &ProjectSettings::to_json(jsonParser &json) const {

    json = jsonParser::object();

    json["name"] = name();
    json["curr_properties"] = properties();
    json["curr_clex"] = clex_name();
    json["curr_calctype"] = calctype();
    json["curr_ref"] = ref();
    json["curr_bset"] = bset();
    json["curr_eci"] = eci();
    json["nlist_weight_matrix"] = nlist_weight_matrix();
    json["nlist_sublat_indices"] = nlist_sublat_indices();


    auto _write_if = [&](std::string name, std::string val) {
      if(!val.empty()) {
        json[name] = val;
      };
    };

    _write_if("cxx", m_cxx.first);
    _write_if("cxxflags", m_cxxflags.first);
    _write_if("soflags", m_soflags.first);
    _write_if("casm_prefix", m_casm_prefix.first.string());
    _write_if("boost_prefix", m_boost_prefix.first.string());
    _write_if("compile_options", m_depr_compile_options);
    _write_if("so_options", m_depr_so_options);

    json["view_command"] = view_command();
    json["crystallography_tol"] = crystallography_tol();
    json["crystallography_tol"].set_scientific();
    json["lin_alg_tol"] = lin_alg_tol();
    json["lin_alg_tol"].set_scientific();
    json["query_alias"] = aliases();

    return json;
  }

  namespace {
    std::string _wdefaultval(std::string name, std::pair<std::string, std::string> val) {
      return name + ": '" + val.first + "' (" + val.second + ")\n";
    }

    std::string _wdefaultval(std::string name, std::pair<fs::path, std::string> val) {
      return _wdefaultval(name, std::make_pair(val.first.string(), val.second));
    }
  }

  /// \brief Print summary of ProjectSettings, as for 'casm settings -l'
  void ProjectSettings::print_summary(Log &log) const {

    std::vector<std::string> all = m_dir.all_bset();
    log.custom<Log::standard>("Basis sets");
    for(int i = 0; i < all.size(); i++) {
      log << all[i];
      if(bset() == all[i]) {
        log << "*";
      }
      log << "\n";
    }
    log << std::endl;

    all = m_dir.all_calctype();
    log.custom<Log::standard>("Training Data & References");
    for(int i = 0; i < all.size(); i++) {
      std::vector<std::string> all_ref = m_dir.all_ref(all[i]);
      for(int j = 0; j < all_ref.size(); j++) {
        log << all[i] << " / " << all_ref[j];
        if(calctype() == all[i] && ref() == all_ref[j]) {
          log << "*";
        }
        log << "\n";
      }
      log << "\n";
    }

    all = m_dir.all_eci(clex_name(), calctype(), ref(), bset());
    log.custom<Log::standard>("ECI for current settings");
    for(int i = 0; i < all.size(); i++) {
      log << all[i];
      if(eci() == all[i]) {
        log << "*";
      }
      log << "\n";
    }
    log << std::endl;

    log.custom<Log::standard>("Compiler settings");
    log << _wdefaultval("cxx", cxx())
        << _wdefaultval("cxxflags", cxxflags())
        << _wdefaultval("soflags", soflags())
        << _wdefaultval("casm_prefix", casm_prefix())
        << _wdefaultval("boost_prefix", boost_prefix()) << std::endl;

    if(!m_depr_compile_options.empty()) {
      log << "** using 'compile_options' from .casm/project_settings.json **\n";
    }
    log << "compile command: '" << compile_options() << "'\n\n";

    if(!m_depr_so_options.empty()) {
      log << "** using 'so_options' from .casm/project_settings.json **\n";
    }
    log << "so command: '" << so_options() << "'\n\n";

    log.custom<Log::standard>("'casm view'");
    log << "command: '" << view_command() << "'\n\n";

  }

  jsonParser &to_json(const ProjectSettings &set, jsonParser &json) {
    return set.to_json(json);
  }

}

