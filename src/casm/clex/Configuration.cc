#include "casm/clex/Configuration.hh"

#include <sstream>
//#include "casm/misc/Time.hh"
#include "casm/clex/PrimClex.hh"
#include "casm/clex/Supercell.hh"
#include "casm/clex/Clexulator.hh"
#include "casm/crystallography/jsonStruc.hh"
#include "casm/clex/ECIContainer.hh"
#include "casm/casm_io/VaspIO.hh"

namespace CASM {

  /// Construct a default Configuration
  Configuration::Configuration(Supercell &_supercell, const jsonParser &src, const ConfigDoF &_configdof)
    : id("none"), supercell(&_supercell), source_updated(true), multiplicity(-1),
      m_configdof(_configdof), prop_updated(true), m_selected(false) {
    set_source(src);
  }

  //*********************************************************************************
  /// Construct by reading from main data file (json)
  Configuration::Configuration(const jsonParser &json, Supercell &_supercell, Index _id)
    : supercell(&_supercell), source_updated(false), multiplicity(-1),
      m_configdof(_supercell.num_sites()), prop_updated(false) {

    std::stringstream ss;
    ss << _id;
    id = ss.str();

    read(json);
  }

  //*********************************************************************************
  /*
  /// Construct a Configuration with occupation specified by string 'con_name'
  Configuration::Configuration(Supercell &_supercell, std::string con_name, bool select, const jsonParser &src)
    : id("none"), supercell(&_supercell), source_updated(true), multiplicity(-1),
      m_configdof(_supercell.num_sites()), prop_updated(true), m_selected(select) {

    set_source(src);

    // just use bitstring, limit max occupation to 9
    if(con_name.size() != _supercell.num_sites()) {
      std::cout << "Error in Configuration::Configuration(const Supercell&, std::string, bool)." << std::endl;
      std::cout << "  Configuration size _supercell.num_sites(): " << _supercell.num_sites() << "  does not match con_name: '" << con_name << "'." << std::endl;
      exit(1);
    }

    m_configdof.set_occupation(Array<int>(size()));

    for(Index i = 0; i < size(); i++) {
      _occ(i) = con_name[i] - '0';

      if(occ(i) < 0 || occ(i) > 9) {
        std::cout << "Error in Configuration::Configuration(const Supercell&, std::string, bool)." << std::endl;
        std::cout << "  Occupant not allowed: '" << con_name[i] << "'." << std::endl;
        exit(1);
      }
    }

    return;
  }
  */
  //********** MUTATORS  ***********

  void Configuration::set_id(Index _id) {
    std::stringstream ss;
    ss << _id;
    id = ss.str();

    prop_updated = true;
  }

  //*********************************************************************************
  void Configuration::set_source(const jsonParser &source) {
    if(source.is_null() || source.size() == 0) {
      m_source.put_array();
    }
    else if(!source.is_array()) {
      m_source.put_array();
      m_source.push_back(source);
    }
    else {
      m_source = source;
    }
    source_updated = true;
  }

  //*********************************************************************************
  void Configuration::push_back_source(const jsonParser &source) {

    if(source.is_null() || source.size() == 0) {
      return;
    }
    if(!source.is_array()) {

      // check if the new source is already listed, if it is do nothing
      for(int i = 0; i < m_source.size(); i++) {
        if(m_source[i] == source)
          return;
      }

      // else, add the new source
      m_source.push_back(source);

      source_updated = true;
    }
    else {

      // check all new sources, if already listed skip, if the any of the new sources is already listed, if it is do nothing

      for(int s = 0; s < source.size(); s++) {

        bool found = false;

        for(int i = 0; i < m_source.size(); i++) {
          if(m_source[i] == source[s]) {
            found = true;
            break;
          }
        }

        if(!found) {
          // else, add the new source
          m_source.push_back(source[s]);

          source_updated = true;
        }
      }
    }
  }

  //*********************************************************************************
  void Configuration::set_occupation(const Array<int> &new_occupation) {
    id = "none";
    m_configdof.set_occupation(new_occupation);
    return;
  }

  //*********************************************************************************
  void Configuration::set_occ(Index site_l, int val) {
    //if( i > occupation.size() || i < 0)
    //{
    //    std::cout << "ERROR in Configuration::set_occ(). i: " << i << " occupation.size(): "<< occupation.size() << "  val: " << val << std::endl;
    //    exit(1);
    //}
    //std::cout << "Configuration::set_occ(). i: " << i << " occupation.size(): "<< occupation.size() << "  val: " << val << std::endl;
    id = "none";
    m_configdof.occ(site_l) = val;
  }

  //*********************************************************************************

  void Configuration::set_displacement(const displacement_matrix_t &new_displacement) {
    id = "none";
    m_configdof.set_displacement(new_displacement);
  }

  //*********************************************************************************

  void Configuration::set_deformation(const Eigen::Matrix3d &new_deformation) {
    id = "none";
    m_configdof.set_deformation(new_deformation);
  }

  //*********************************************************************************

  Configuration Configuration::canonical_form(PermuteIterator it_begin, PermuteIterator it_end, PermuteIterator &it_canon, double tol) const {
    Configuration tconfig(*this);
    tconfig.m_configdof = m_configdof.canonical_form(it_begin, it_end, it_canon, tol);
    return tconfig;
  }

  //*********************************************************************************
  void Configuration::set_calc_properties(const jsonParser &calc) {
    prop_updated = true;
    calculated = calc;
  }

  //*********************************************************************************

  bool Configuration::read_calc_properties(jsonParser &parsed_props) const {
    //std::cout << "begin Configuration::read_calculated()" << std::endl;
    bool success = true;
    /// properties.calc.json: contains calculated properties
    ///   For default clex calctype only
    fs::path filepath = calc_properties_path();
    //std::cout << "filepath: " << filepath << std::endl;
    parsed_props = jsonParser();
    if(fs::exists(filepath)) {
      jsonParser json(filepath);

      //Record file timestamp
      parsed_props["data_timestamp"] = fs::last_write_time(filepath);

      std::vector<std::string> props = get_primclex().settings().properties();
      for(Index i = 0; i < props.size(); i++) {
        //std::cout << "checking for: " << props[i] << std::endl;
        if(json.contains(props[i])) {

          // normal by #prim cells for some properties
          if(props[i] == "energy" || props[i] == "relaxed_energy") {
            parsed_props[ props[i] ] = json[props[i]].get<double>() / get_supercell().volume();
          }
          else {
            parsed_props[props[i]] = json[props[i]];
          }
        }
        else
          success = false;
      }
      //Get RMS force:
      if(json.contains("relaxed_forces")) {
        if(json["relaxed_forces"].size()) {
          Eigen::MatrixXd forces;
          from_json(forces, json["relaxed_forces"]);
          parsed_props["rms_force"] = sqrt((forces.transpose() * forces).trace() / double(forces.rows()));
        }
        else {
          parsed_props["rms_force"] = 0.;
        }
      }
    }
    else
      success = false;

    return success;
  }

  //********** ACCESSORS ***********

  const Lattice &Configuration::ideal_lattice()const {
    return get_supercell().get_real_super_lattice();
  }

  //*********************************************************************************

  std::string Configuration::get_id() const {
    return id;
  }

  //*********************************************************************************
  /// \brief SCELV_A_B_C_D_E_F/i
  std::string Configuration::name() const {
    return get_supercell().get_name() + "/" + get_id();
  }

  //*********************************************************************************
  /// \brief SCELV_A_B_C_D_E_F.i (only use this when 'name' won't work)
  std::string Configuration::altname() const {
    return get_supercell().get_name() + "." + get_id();
  }

  //*********************************************************************************
  std::string Configuration::calc_status() const {
    if(fs::exists(calc_status_path())) {
      jsonParser json(calc_status_path());
      if(json.contains("status"))
        return json["status"].get<std::string>();
    }
    return("not_submitted");
  }

  //*********************************************************************************
  std::string Configuration::failure_type() const {
    if(fs::exists(calc_status_path())) {
      jsonParser json(calc_status_path());
      if(json.contains("failure_type"))
        return json["failure_type"].get<std::string>();
    }
    return("none");
  }

  //*********************************************************************************
  const jsonParser &Configuration::source() const {
    return m_source;
  }

  //*********************************************************************************
  fs::path Configuration::get_path() const {
    return get_supercell().get_path() / get_id();
  }

  //*********************************************************************************
  ///Returns number of sites, NOT the number of primitives that fit in here
  Index Configuration::size() const {
    return get_supercell().num_sites();
  }

  //*********************************************************************************
  const Structure &Configuration::get_prim() const {
    return get_supercell().get_prim();
  }

  //*********************************************************************************
  //PrimClex &Configuration::get_primclex() {
  //return get_supercell().get_primclex();
  //}

  //*********************************************************************************
  const PrimClex &Configuration::get_primclex() const {
    return get_supercell().get_primclex();
  }

  //*********************************************************************************
  Supercell &Configuration::get_supercell() {
    return *supercell;
  }

  //*********************************************************************************
  const Supercell &Configuration::get_supercell() const {
    return *supercell;
  }

  //*********************************************************************************
  UnitCellCoord Configuration::get_uccoord(Index site_l) const {
    return get_supercell().uccoord(site_l);
  }

  //*********************************************************************************
  int Configuration::get_b(Index site_l) const {
    return get_supercell().get_b(site_l);
  }

  //*********************************************************************************
  const Molecule &Configuration::get_mol(Index site_l) const {
    return get_prim().basis[ get_b(site_l) ].site_occupant()[ occ(site_l) ];
  }

  //*********************************************************************************
  const Properties &Configuration::calc_properties() const {
    return calculated;
  }

  //*********************************************************************************
  /*
    const DeltaProperties &Configuration::delta_properties() const {
      return delta;
    }
  */
  //*********************************************************************************

  const Properties &Configuration::generated_properties() const {
    return generated;
  }

  //*********************************************************************************

  /*const Correlation &Configuration::get_correlations() const {
    return correlations;
    }*/

  //*********************************************************************************

  /// Returns composition on each sublattice: sublat_comp[ prim basis site / sublattice][ molecule_type]
  ///   molucule_type is ordered as in the Prim structure's site_occupant list for that basis site (includes vacancies)
  ReturnArray< Array < double > > Configuration::get_sublattice_composition() const {

    // get the number of each molecule
    Array< Array < int > > sublat_num_each_molecule = get_sublat_num_each_molecule();
    Array< Array < double > > sublattice_composition(sublat_num_each_molecule.size());

    // divide by number of sites per sublattice ( supercell volume )
    for(Index i = 0; i < sublat_num_each_molecule.size(); i++) {
      sublattice_composition[i].resize(sublat_num_each_molecule[i].size());
      for(Index j = 0; j < sublat_num_each_molecule[i].size(); j++) {
        sublattice_composition[i][j] = (1.0 * sublat_num_each_molecule[i][j]) / get_supercell().volume();
      }
    }

    return sublattice_composition;
  }

  //*********************************************************************************
  /// Returns number of each molecule by sublattice:
  ///   sublat_num_each_molecule[ prim basis site / sublattice ][ molecule_type]
  ///   molucule_type is ordered as in the Prim structure's site_occupant list for that basis site
  ReturnArray< Array<int> > Configuration::get_sublat_num_each_molecule() const {

    Index i;

    // [basis_site][site_occupant_index]
    auto convert = get_index_converter(get_prim(), get_prim().get_struc_molecule());

    // create an array to count the number of each molecule
    Array< Array<int> > sublat_num_each_molecule;
    for(i = 0; i < get_prim().basis.size(); i++) {
      sublat_num_each_molecule.push_back(Array<int>(get_prim().basis[i].site_occupant().size(), 0));
    }

    // count the number of each molecule by sublattice
    for(i = 0; i < size(); i++) {
      sublat_num_each_molecule[ get_b(i) ][occ(i)]++;
    }

    return sublat_num_each_molecule;
  }

  //*********************************************************************************
  /// Returns composition, not counting vacancies
  ///    composition[ molecule_type ]: molecule_type ordered as prim structure's get_struc_molecule(), with [Va]=0.0
  ReturnArray<double> Configuration::get_composition() const {

    // get the number of each molecule type
    Array<int> num_each_molecule = get_num_each_molecule();

    /// get the total number of non-vacancy atoms
    int num_atoms = 0;

    // need to know which molecules are vacancies
    auto struc_molecule = get_prim().get_struc_molecule();

    Index i;
    for(i = 0; i < struc_molecule.size(); i++) {
      if(struc_molecule[i].is_vacancy()) {
        // set to zero, so the Va concentration is reported as 0.0
        num_each_molecule[i] = 0;
      }
      num_atoms += num_each_molecule[i];
    }

    // calculate the comp (not including vacancies) from the number of each molecule
    Array<double> comp;
    for(i = 0; i < num_each_molecule.size(); i++)
      comp.push_back((1.0 * num_each_molecule[i]) / double(num_atoms));

    return comp;

  }

  //*********************************************************************************
  /// Returns composition, including vacancies
  ///    composition[ molecule_type ]: molecule_type ordered as prim structure's get_struc_molecule()
  ReturnArray<double> Configuration::get_true_composition() const {

    Array<int> num_each_molecule = get_num_each_molecule();

    // calculate the true_comp (including vacancies) from the number of each molecule
    Array<double> comp;
    for(Index i = 0; i < num_each_molecule.size(); i++)
      comp.push_back((1.0 * num_each_molecule[i]) / size());

    return comp;
  }

  //*********************************************************************************
  /// Returns num_each_molecule[ molecule_type], where 'molecule_type' is ordered as Structure::get_struc_molecule()
  ReturnArray<int> Configuration::get_num_each_molecule() const {
    return CASM::get_num_each_molecule(m_configdof, get_supercell());
  }

  //*********************************************************************************
  /// Returns parametric composition, as calculated using PrimClex::param_comp
  Eigen::VectorXd Configuration::get_param_composition() const {
    if(!get_primclex().has_composition_axes()) {
      std::cerr << "Error in Configuration::get_param_composition()" << std::endl;
      std::cerr << "  Composition axes are not set." << std::endl;
      exit(1);
    }

    return get_primclex().composition_axes().param_composition(get_num_each_component());
  }

  //*********************************************************************************
  /// Returns num_each_component[ component_type] per prim cell,
  ///   where 'component_type' is ordered as ParamComposition::get_components
  Eigen::VectorXd Configuration::get_num_each_component() const {

    // component order used for param_composition
    std::vector<std::string> v_components = get_primclex().composition_axes().components();

    // copy to CASM::Array
    std::vector<std::string> components;
    for(auto it = v_components.cbegin(); it != v_components.cend(); ++it) {
      components.push_back(*it);
    }

    // initialize
    Eigen::VectorXd num_each_component = Eigen::VectorXd::Zero(components.size());

    // [basis_site][site_occupant_index]
    auto convert = get_index_converter(get_prim(), components);

    // count the number of each component
    for(Index i = 0; i < size(); i++) {
      num_each_component[ convert[ get_b(i) ][occ(i)] ] += 1.0;
    }

    // normalize per prim cell
    for(Index i = 0; i < components.size(); i++) {
      num_each_component[i] /= get_supercell().volume();
    }

    return num_each_component;
  }



  //********* IO ************


  /// Writes the Configuration to a json object (the config list)
  ///   Uses PrimClex's current default settings to write the appropriate properties
  ///
  ///   'json' is a jsonParser JSON object (or will be set to a JSON object)
  ///
  ///   write_dof, source, selected: (absolute path in config_list)
  ///     json["supercells"]["SCEL_NAME"]["CONFIG_ID"]["dof"]
  ///     json["supercells"]["SCEL_NAME"]["CONFIG_ID"]["source"]
  ///     json["supercells"]["SCEL_NAME"]["CONFIG_ID"]["selected"]
  ///
  ///   write_properties: (absolute path in config_list)
  ///     json["supercells"]["SCEL_NAME"]["CONFIG_ID"]["CURR_CALCTYPE"]["CURR_REF"]["properties"]["calc"]
  ///     json["supercells"]["SCEL_NAME"]["CONFIG_ID"]["CURR_CALCTYPE"]["CURR_REF"]["properties"]["generated"]
  jsonParser &Configuration::write(jsonParser &json) const {

    //std::cout << "begin Configuration::write()" << std::endl;

    const ProjectSettings &set = get_primclex().settings();
    std::string calc_string = "calctype." + set.default_clex().calctype;
    std::string ref_string = "ref." + set.default_clex().ref;

    /// write json object hierarchy if not existing
    jsonParser &json_scel = json["supercells"][get_supercell().get_name()];
    jsonParser &json_config = json_scel[get_id()];
    jsonParser &json_ref = json_config[calc_string][ref_string];
    jsonParser &json_prop = json_ref["properties"];

    json_config["selected"] = selected();

    if(!json_config.contains("dof")) {
      write_dof(json_config);
    }

    if(source_updated) {
      write_source(json_config);
    }

    if(prop_updated) {
      write_properties(json_prop);
    }

    //std::cout << "finish Configuration::write()" << std::endl;

    return json;
  }

  //*********************************************************************************

  void Configuration::write_pos() const {

    try {
      fs::create_directories(get_path());
    }
    catch(const fs::filesystem_error &ex) {
      std::cerr << "Error in Configuration::write_pos()." << std::endl;
      std::cerr << ex.what() << std::endl;
    }

    fs::ofstream file(get_pos_path());
    VaspIO::PrintPOSCAR p(*this);
    p.sort();
    p.print(file);
    return;
  }

  //*********************************************************************************

  void Configuration::print_occupation(std::ostream &stream) const {
    stream << occupation() << "\n";
    return;
  }

  //*********************************************************************************

  void Configuration::print_config_list(std::ostream &stream, int composition_flag) const {

    stream.width(10);
    stream.flags(std::ios::left);
    stream << id << " ";

    stream.width(10);
    stream.flags(std::ios::showpoint | std::ios::fixed | std::ios::left);
    stream << name() << " ";
    //Prints composition if comp_flag=1, true_composition if comp_flag=2
    // and the sublattice composition if comp_flag=3
    if(composition_flag == 1) {
      print_composition(stream);
    }
    else if(composition_flag == 2) {
      print_true_composition(stream);
    }
    else if(composition_flag == 3) {
      print_sublattice_composition(stream);
    }

    stream.width(8);
    stream.flags(std::ios::showpoint | std::ios::fixed | std::ios::right);
    stream << selected();

    stream << "\n";
  }

  //*********************************************************************************
  void Configuration::print_composition(std::ostream &stream) const {

    Array<double> comp = get_composition();
    auto mol_list = get_prim().get_struc_molecule();

    for(Index i = 0; i < mol_list.size(); i++) {
      if(mol_list[i].is_vacancy()) {
        continue;
      }
      stream.precision(6);
      stream.width(12);
      stream.flags(std::ios::showpoint | std::ios::fixed | std::ios::right);
      stream << comp[i] << " ";
    }

  }

  //*********************************************************************************
  void Configuration::print_true_composition(std::ostream &stream) const {

    Array<double> true_comp = get_true_composition();

    for(Index i = 0; i < true_comp.size(); i++) {
      stream.precision(6);
      stream.width(12);
      stream.flags(std::ios::showpoint | std::ios::fixed | std::ios::right);
      stream << true_comp[i] << " ";
    }

  }

  //*********************************************************************************
  void Configuration::print_sublattice_composition(std::ostream &stream) const {

    Array< Array<double> > sublattice_composition = get_sublattice_composition();

    for(Index i = 0; i < sublattice_composition.size(); i++) {
      for(Index j = 0; j < sublattice_composition[i].size(); j++) {
        stream.precision(6);
        stream.width(12);
        stream.flags(std::ios::showpoint | std::ios::fixed | std::ios::right);
        stream << sublattice_composition[i][j] << " ";
      }
    }

  }

  //*********************************************************************************

  /// Private members:

  /// Reads the Configuration from the config list
  ///   Uses PrimClex's current default settings to read in the appropriate properties
  ///
  /// This is private, because it is only called from the constructor:
  ///   Configuration(const Supercell &_supercell, Index _id)
  ///   It's called from the constructor because of the Supercell pointer
  ///
  ///   read dof, source, selected: (absolute path in config_list)
  ///     json["supercells"]["SCEL_NAME"]["CONFIG_ID"]["dof"]
  ///     json["supercells"]["SCEL_NAME"]["CONFIG_ID"]["source"]
  ///     json["supercells"]["SCEL_NAME"]["CONFIG_ID"]["selected"]
  ///
  ///   read properties: (absolute path in config_list)
  ///     json["supercells"]["SCEL_NAME"]["CONFIG_ID"]["CURR_CALCTYPE"]["CURR_REF"]["properties"]["calc"]
  ///     json["supercells"]["SCEL_NAME"]["CONFIG_ID"]["CURR_CALCTYPE"]["CURR_REF"]["properties"]["generated"]
  ///
  void Configuration::read(const jsonParser &json) {

    //std::cout << "begin  Configuration::read()" << std::endl;

    const ProjectSettings &set = get_primclex().settings();

    std::string calc_string = "calctype." + set.default_clex().calctype;
    std::string ref_string = "ref." + set.default_clex().ref;

    // read dof
    if(!json.contains("supercells"))
      return;
    const jsonParser &json_scel = json["supercells"];
    if(!json_scel.contains(get_supercell().get_name()))
      return;
    if(!json_scel[get_supercell().get_name()].contains(get_id()))
      return;
    const jsonParser &json_config = json_scel[get_supercell().get_name()][get_id()];

    read_dof(json_config);


    // read properties: does not attempt to read in new calculation data
    if(!json_config.contains(calc_string))
      return;
    const jsonParser &json_calc = json_config[calc_string];
    if(!json_calc.contains(ref_string))
      return;
    const jsonParser &json_ref = json_calc[ref_string];
    if(!json_ref.contains("properties"))
      return;
    const jsonParser &json_prop = json_ref["properties"];

    read_properties(json_prop);

    //std::cout << "finish Configuration::read()" << std::endl;
  }

  //*********************************************************************************

  /// Read degree of freedom, source, and selected info
  ///
  ///   read dof, source, selected:  (absolute path in config_list)
  ///     json["supercells"]["SCEL_NAME"]["CONFIG_ID"]["dof"]
  ///     json["supercells"]["SCEL_NAME"]["CONFIG_ID"]["source"]
  ///     json["supercells"]["SCEL_NAME"]["CONFIG_ID"]["selected"]
  ///
  /// Tries to read dof from json["dof"]
  /// Tries to read source from json["source"]
  /// Tries to read selected from json["selected"]
  ///
  void Configuration::read_dof(const jsonParser &json) {

    /// json["dof"]: contains degree of freedom information
    if(!json.contains("dof")) {
      id = "none";
      set_selected(false);
      return;
    }
    else {

      json.get_if(m_source, "source");
      json.get_else(m_selected, "selected", false);
      from_json(m_configdof, json["dof"]);
    }
  }

  //*********************************************************************************
  /// Read configuration properties
  /// - this does not automatically read new externally calculated properties
  ///
  ///   read properties: (absolute path in config_list)
  ///     json["supercells"]["SCEL_NAME"]["CONFIG_ID"]["CURR_CALCTYPE"]["CURR_REF"]["properties"]["calc"]
  ///     json["supercells"]["SCEL_NAME"]["CONFIG_ID"]["CURR_CALCTYPE"]["CURR_REF"]["properties"]["generated"]
  ///
  /// Tries to read calculated from json["calc"]
  /// Tries to read generated from json["gen"]
  ///
  void Configuration::read_properties(const jsonParser &json) {
    if(json.contains("calc")) {
      from_json(calculated, json["calc"]);
    }

    if(json.contains("gen")) {
      from_json(generated, json["gen"]);
    }
  }

  //*********************************************************************************
  fs::path Configuration::get_pos_path() const {
    return get_primclex().dir().POS(name());
  }

  //*********************************************************************************
  fs::path Configuration::calc_properties_path() const {
    return get_primclex().dir().calculated_properties(name(), get_primclex().settings().default_clex().calctype);
  }

  //*********************************************************************************
  fs::path Configuration::calc_status_path() const {
    return get_primclex().dir().calc_status(name(), get_primclex().settings().default_clex().calctype);
  }

  //*********************************************************************************
  /// Write config.json file containing degree of freedom info
  ///
  ///   writes dof: (absolute path in config_list)
  ///     json["supercells"]["SCEL_NAME"]["CONFIG_ID"]["dof"]
  ///
  ///   adds: json["dof"]
  ///
  jsonParser &Configuration::write_dof(jsonParser &json) const {

    if(!json["dof"].is_obj()) {
      json["dof"].put_obj();
    }

    jsonParser &dof = json["dof"];

    if(occupation().size() == 0) {
      dof.erase("occupation");
    }
    else {
      dof["occupation"] = occupation();
    }

    if(displacement().size() == 0) {
      dof.erase("displacement");
    }
    else {
      dof["displacement"] = displacement();
    }

    if(!is_strained()) {
      dof.erase("deformation");
    }
    else {
      dof["deformation"] = deformation();
    }

    return json;

  }

  //*********************************************************************************
  /// Write config.json file containing degree of freedom info
  ///   location: json = supercells/SCEL_NAME/CONFIG_ID, adds: source
  ///
  jsonParser &Configuration::write_source(jsonParser &json) const {

    json["source"] = m_source;

    return json;

  }

  //*********************************************************************************
  /// Write POS file containing Structure
  ///   location: json = supercells/SCEL_NAME/CONFIG_ID, adds: pos
  ///   If the configuration is completely vacant, json["pos"] = null
  ///
  jsonParser &Configuration::write_pos(jsonParser &json) const {

    // print POS to stringstream
    if(occupation() != get_supercell().vacant()) {
      std::stringstream ss;
      VaspIO::PrintPOSCAR p(*this);
      p.sort();
      p.print(ss);

      json["pos"] = ss.str();
    }
    else {
      json["pos"].put_null();
    }

    return json;

  }

  //*********************************************************************************
  /// Write param_composition.json file containing correlations
  ///   location: json = supercells/SCEL_NAME/CONFIG_ID/CURR_CLEX/CURR_REF, adds: param_composition
  ///
  jsonParser &Configuration::write_param_composition(jsonParser &json) const {

    if(!get_primclex().has_composition_axes()) {
      json.erase("param_comp_formula");
      json.erase("param_composition");
      return json;
    }

    json["param_comp_formula"] = get_primclex().composition_axes().mol_formula();
    json["param_composition"] = get_param_composition();

    return json;

  }

  //*********************************************************************************
  /// Write properties.calc.json file containing calculated properties and param_composition
  ///   location: json = supercells/SCEL_NAME/CONFIG_ID/CURR_CLEX/CURR_REF/properties, adds: calc
  ///
  jsonParser &Configuration::write_properties(jsonParser &json) const {

    if(calculated.size() == 0) {
      json.erase("calc");
    }
    else {
      json["calc"] = calculated;
    }

    if(generated.size() == 0) {
      json.erase("gen");
    }
    else {
      json["gen"] = generated;
    }

    return json;

  }

  //*********************************************************************************
  //--------------------------------------------------------------------------------------------------
  //Structure Factor
  Eigen::VectorXd Configuration::get_struct_fact_intensities() const {
    Eigen::VectorXd automatic_intensities(get_prim().get_struc_molecule().size());
    for(int i = 0; i < get_prim().get_struc_molecule().size(); i++)
      automatic_intensities(i) = i;
    return get_struct_fact_intensities(automatic_intensities);
  }

  Eigen::VectorXd Configuration::get_struct_fact_intensities(const Eigen::VectorXd &component_intensities) const {
    auto convert = get_index_converter(get_prim(), get_prim().get_struc_molecule());
    Eigen::VectorXd intensities(size());
    for(int i = 0; i < size(); i++) {
      intensities(i) = component_intensities(convert[get_b(i)][occ(i)]);
    }
    return intensities;
  }

  ///  Calculates the sublattice structure factors as:
  ///       intensities.segment<volume()>(i*volume()) * fourier_matrix = Q.column(i)
  ///       Q is arranged as: [Q1(k1) Q2(k1) ... Qn(k1)]
  ///                         [Q1(k2) Q2(k2) ... Qn(k2)]
  ///                         ...
  ///                         [Q1(kn) Q2(kn) ... Qn(kn)]
  ///  Q is called sublat_sf in the code
  void Configuration::calc_sublat_struct_fact(const Eigen::VectorXd &intensities) {
    //std::cout<<"Intensities"<<std::endl<<intensities<<std::endl;
    Eigen::MatrixXcd sublat_sf(supercell->basis_size(), supercell->fourier_matrix().cols());
    if(supercell->fourier_matrix().rows() == 0 || supercell->fourier_matrix().cols() == 0) {
      std::cerr << "ERROR in Configuration::calc_sublat_struct_fact. Did you "
                << "forget to initialize a fourier matrix in Supercell?"
                << " Quitting" << std::endl;
      exit(666);
    }
    //int num_kpoints = supercell->fourier_matrix().cols();
    int supercell_volume = supercell->volume();
    for(int i = 0; i < supercell->basis_size(); i++) {
      Eigen::VectorXd int_segment = intensities.segment(i * supercell_volume, supercell_volume);
      // std::cout<<"Intensity segment:"<<int_segment.transpose()<<std::endl;
      // std::cout<<"Fourier:"<<std::endl<<supercell->fourier_matrix()<<std::endl;
      sublat_sf.row(i) = int_segment.transpose() * supercell->fourier_matrix();
    }
    sublat_sf = sublat_sf / double(supercell->volume());
    // std::cout<<"Sublattice struct factor:"<<std::endl;
    // std::cout<<sublat_sf.transpose()<<std::endl;
    generated["sublat_struct_fact"] =  sublat_sf.transpose();
    prop_updated = true;
  }

  /// Structure factors are then calculated as S:
  /// S = (Q * m_phase_factor).diagonal().absolute_value()
  /// S is arranged as: [S(k1) S(k2) ... S(kn)]
  /// In the code: Q is called sublat_sf
  /// However, it would be useful to have a matrix that contained the coordinates of the k-points
  /// along with the intensities at those points. The matrix that is stored in generated is thus
  /// formatted as:
  ///   [k_x  k_y  k_z  S(k)]
  void Configuration::calc_struct_fact(const Eigen::VectorXd &intensities) {
    if(supercell->phase_factor().rows() == 0 || supercell->phase_factor().cols() == 0) {
      std::cerr << "ERROR in Configuration::calc_struct_fact. Did you "
                << "forget to initialize a phase-factor matrix in Supercell?"
                << " Quitting" << std::endl;
      exit(666);
    }
    calc_sublat_struct_fact(intensities);
    Eigen::MatrixXcd sublat_sf = generated["sublat_struct_fact"].get<Eigen::MatrixXcd>();
    //    std::cout<<"Multiplication matrix:"<<std::endl<<sublat_sf*supercell->phase_factor()<<std::endl;
    Eigen::VectorXcd raw_amplitudes = (sublat_sf * supercell->phase_factor()).diagonal();
    // std::cout.precision(4);
    // std::cout<<std::fixed;
    // std::cout<<"Raw amplitudes:"<<std::endl<<raw_amplitudes.transpose()<<std::endl;
    Eigen::MatrixXd sf_coords(raw_amplitudes.size(), 4);
    sf_coords.leftCols(3) = supercell->k_mesh();
    sf_coords.col(3) = raw_amplitudes.cwiseAbs().transpose() / double(supercell->basis_size());
    generated["struct_fact"] = sf_coords;
    prop_updated = true;
  }

  void Configuration::calc_struct_fact() {
    calc_struct_fact(get_struct_fact_intensities());
  }

  void Configuration::calc_sublat_struct_fact() {
    calc_sublat_struct_fact(get_struct_fact_intensities());
  }

  Eigen::MatrixXd Configuration::struct_fact() {
    if(!generated.contains("struct_fact"))
      calc_struct_fact();
    return generated["struct_fact"].get<Eigen::MatrixXd>();
  }

  Eigen::MatrixXcd Configuration::sublat_struct_fact() {
    if(!generated.contains("sublat_struct_fact"))
      calc_sublat_struct_fact();
    return generated["sublat_struct_fact"].get<Eigen::MatrixXcd>();
  }

  /// \brief Returns correlations using 'clexulator'.
  Correlation correlations(const Configuration &config, Clexulator &clexulator) {
    return correlations(config.configdof(), config.get_supercell(), clexulator);
  }

  /// Returns parametric composition, as calculated using PrimClex::param_comp
  Eigen::VectorXd comp(const Configuration &config) {
    return config.get_param_composition();
  }

  /// \brief Returns the composition, as number of each species per unit cell
  Eigen::VectorXd comp_n(const Configuration &config) {
    return config.get_num_each_component();
  }

  /// \brief Returns the vacancy composition, as number per unit cell
  double n_vacancy(const Configuration &config) {
    if(config.get_primclex().vacancy_allowed()) {
      return comp_n(config)[config.get_primclex().vacancy_index()];
    }
    return 0.0;
  }

  /// \brief Returns the total number species per unit cell
  ///
  /// Equivalent to \code comp_n(config).sum() - n_vacancy(config) \endcode
  double n_species(const Configuration &config) {
    return comp_n(config).sum() - n_vacancy(config);
  }

  /// \brief Returns the composition as species fraction, with [Va] = 0.0, in the order of Structure::get_struc_molecule
  ///
  /// - Currently, this is really a Molecule fraction
  Eigen::VectorXd species_frac(const Configuration &config) {
    Eigen::VectorXd v = comp_n(config);
    if(config.get_primclex().vacancy_allowed()) {
      v(config.get_primclex().vacancy_index()) = 0.0;
    }
    return v / v.sum();
  }

  /// \brief Returns the composition as site fraction, in the order of Structure::get_struc_molecule
  Eigen::VectorXd site_frac(const Configuration &config) {
    return comp_n(config) / config.get_prim().basis.size();
  }

  /// \brief Returns the relaxed energy, normalized per unit cell
  double relaxed_energy(const Configuration &config) {
    return config.calc_properties()["relaxed_energy"].get<double>();
  }

  /// \brief Returns the relaxed energy, normalized per species
  double relaxed_energy_per_species(const Configuration &config) {
    return relaxed_energy(config) / n_species(config);
  }

  /// \brief Returns the reference energy, normalized per unit cell
  double reference_energy(const Configuration &config) {
    return reference_energy_per_species(config) * n_species(config);
  }

  /// \brief Returns the reference energy, normalized per species
  ///
  /// - Currently, this is per Molecule
  double reference_energy_per_species(const Configuration &config) {
    return config.get_primclex().chemical_reference()(config);
  }

  /// \brief Returns the formation energy, normalized per unit cell
  double formation_energy(const Configuration &config) {
    return relaxed_energy(config) - reference_energy(config);
  }

  /// \brief Returns the formation energy, normalized per species
  ///
  /// - Currently, this is really a Molecule fraction
  double formation_energy_per_species(const Configuration &config) {
    return formation_energy(config) / n_species(config);
  }

  /// \brief Returns the formation energy, normalized per unit cell
  double clex_formation_energy(const Configuration &config) {
    const auto &primclex = config.get_primclex();
    auto formation_energy = primclex.settings().clex("formation_energy");
    Clexulator clexulator = primclex.clexulator(formation_energy);
    const ECIContainer &eci = primclex.eci(formation_energy);

    if(eci.index().back() >= clexulator.corr_size()) {
      Log &err_log = default_err_log();
      err_log.error<Log::standard>("bset and eci mismatch");
      err_log << "using cluster expansion: 'formation_energy'" << std::endl;
      err_log << "basis set size: " << clexulator.corr_size() << std::endl;
      err_log << "max eci index: " << eci.index().back() << std::endl;
      throw std::runtime_error("Error: bset and eci mismatch");
    }

    return eci * correlations(config, clexulator);
  }

  /// \brief Returns the formation energy, normalized per unit cell
  double clex_formation_energy_per_species(const Configuration &config) {
    return clex_formation_energy(config) / n_species(config);
  }

  /// \brief Return true if all current properties have been been calculated for the configuration
  bool is_calculated(const Configuration &config) {
    const auto &set = config.get_primclex().settings();
    return std::all_of(set.properties().begin(),
                       set.properties().end(),
    [&](const std::string & key) {
      return config.calc_properties().contains(key);
    });
  }

  /// \brief Root-mean-square forces of relaxed configurations, determined from DFT (eV/Angstr.)
  double rms_force(const Configuration &_config) {
    return _config.calc_properties()["rms_force"].get<double>();
  }

  /// \brief Cost function that describes the degree to which basis sites have relaxed
  double basis_deformation(const Configuration &_config) {
    return _config.calc_properties()["basis_deformation"].get<double>();
  }

  /// \brief Cost function that describes the degree to which lattice has relaxed
  double lattice_deformation(const Configuration &_config) {
    return _config.calc_properties()["lattice_deformation"].get<double>();
  }

  /// \brief Change in volume due to relaxation, expressed as the ratio V/V_0
  double volume_relaxation(const Configuration &_config) {
    return _config.calc_properties()["volume_relaxation"].get<double>();
  }

  /// \brief returns true if _config describes primitive cell of the configuration it describes
  bool is_primitive(const Configuration &_config) {
    return _config.is_primitive(_config.get_supercell().permute_begin());
  }

  /// \brief returns true if _config no symmetry transformation applied to _config will increase its lexicographic order
  bool is_canonical(const Configuration &_config) {
    return _config.is_canonical(_config.get_supercell().permute_begin(), _config.get_supercell().permute_end());
  }

  bool has_relaxed_energy(const Configuration &_config) {
    return _config.calc_properties().contains("relaxed_energy");
  }

  bool has_reference_energy(const Configuration &_config) {
    return _config.get_primclex().has_composition_axes() &&
           _config.get_primclex().has_chemical_reference();
  }

  bool has_formation_energy(const Configuration &_config) {
    return has_relaxed_energy(_config) && has_reference_energy(_config);
  }

  bool has_rms_force(const Configuration &_config) {
    return _config.calc_properties().contains("rms_force");
  }

  bool has_basis_deformation(const Configuration &_config) {
    return _config.calc_properties().contains("basis_deformation");
  }

  bool has_lattice_deformation(const Configuration &_config) {
    return _config.calc_properties().contains("lattice_deformation");
  }

  bool has_volume_relaxation(const Configuration &_config) {
    return _config.calc_properties().contains("volume_relaxation");
  }


  /// \brief Application results in filling supercell 'scel' with reoriented motif, op*motif
  ///
  /// Currently only applies to occupation
  Configuration &apply(const ConfigTransform &f, Configuration &motif) {

    Configuration result(f.scel);
    const PrimClex &primclex = motif.get_primclex();
    const Structure &prim = motif.get_prim();

    Lattice oriented_motif_lat = copy_apply(f.op, motif.get_supercell().get_real_super_lattice());

    // Create a PrimGrid linking the prim and the oriented motif each to the supercell
    // So we can tile the decoration of the motif config onto the supercell correctly
    PrimGrid prim_grid(oriented_motif_lat, f.scel.get_real_super_lattice());

    // For each site in the motif, re-orient and then translate by all possible
    // translations from prim_grid, index in the mc_prim_prim, and use index to
    // assign occupation

    // std::vector of occupations in the MonteCarlo cell
    Array<int> tscel_occ(motif.size()*prim_grid.size());

    // for each site in motif
    for(Index s = 0 ; s < motif.size() ; s++) {

      // apply symmetry to re-orient and find unit cell coord
      UnitCellCoord oriented_uccoord = copy_apply(f.op, motif.get_uccoord(s), prim);

      // for each unit cell of the oriented motif in the supercell, copy the occupation
      for(Index i = 0 ; i < prim_grid.size() ; i++) {



        //tscel_occ[f.scel.find(prim_grid.uccoord(i)) = motif.occ(s);

        Index prim_motif_tile_ind = f.scel.prim_grid().find(prim_grid.coord(i, PRIM));

        UnitCellCoord mc_uccoord =  f.scel.prim_grid().uccoord(prim_motif_tile_ind) + oriented_uccoord.unitcell();
        // b-index when doing UnitCellCoord addition is ambiguous; explicitly set it
        mc_uccoord.sublat() = oriented_uccoord.sublat();

        Index occ_ind = f.scel.find(mc_uccoord);

        tscel_occ[occ_ind] = motif.occ(s);
      }
    }

    result.set_occupation(tscel_occ);

    return motif = result;

  }

  /// \brief Order Configuration lexicographically by occuapation
  bool ConfigDoFOccCompare::operator()(const Configuration &A, const Configuration &B) const {
    return std::lexicographical_compare(A.configdof().occupation().begin(),
                                        A.configdof().occupation().end(),
                                        B.configdof().occupation().begin(),
                                        B.configdof().occupation().end());
  }

}


