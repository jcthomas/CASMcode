#include "casm/crystallography/Structure.hh"

#include <sstream>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>

#include "casm/clusterography/SiteCluster.hh"
#include "casm/clusterography/Orbitree.hh"
#include "casm/clusterography/jsonClust.hh"


namespace CASM {
  //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

  Structure::Structure(const fs::path &filepath) : BasicStructure<Site>(), perm_rep_ID(-1), basis_perm_rep_ID(-1) {
    if(!fs::exists(filepath)) {
      std::cerr << "Error in Structure::Structure(const fs::path &filepath)." << std::endl;
      std::cerr << "  File does not exist at: " << filepath << std::endl;
      exit(1);
    }
    fs::ifstream infile(filepath);

    read(infile);
  }

  //***********************************************************

  Structure::Structure(const Structure &RHS) :
    BasicStructure<Site>(RHS) {

    copy_attributes_from(RHS);

  };

  //***********************************************************

  Structure &Structure::operator=(const Structure &RHS) {
    BasicStructure<Site>::operator=(RHS);

    //Following gets done by base class
    //lattice = RHS.lattice;
    //basis = RHS.basis;
    //title = RHS.title;

    /*
    for(Index i = 0; i < basis.size(); i++) {
      basis[i].set_lattice(lattice);
    }
    */

    copy_attributes_from(RHS);

    return *this;
  }


  //***********************************************************

  void Structure::copy_attributes_from(const Structure &RHS) {

    BasicStructure<Site>::copy_attributes_from(RHS);

    SD_flag = RHS.SD_flag;

    perm_rep_ID = RHS.perm_rep_ID; //this *should* work
    basis_perm_rep_ID = RHS.basis_perm_rep_ID; //this *should* work

    factor_group_internal = RHS.factor_group_internal;
    factor_group_internal.set_lattice(lattice(), CART);

  }

  //***********************************************************
  /*
  Structure &Structure::apply_sym(const SymOp &op) { //AAB
    for(Index i = 0; i < basis.size(); i++) {
      basis[i].apply_sym(op);
    }
    return *this;
  }
  */
  //***********************************************************


  void Structure::generate_factor_group_slow(double map_tol) const {
    factor_group_internal.clear();
    BasicStructure<Site>::generate_factor_group_slow(factor_group_internal, map_tol);
    return;
  }

  //************************************************************
  void Structure::generate_factor_group(double map_tol) const {
    factor_group_internal.clear();
    //std::cout << "GENERATING STRUCTURE FACTOR GROUP " << &factor_group_internal << "\n";
    BasicStructure<Site>::generate_factor_group(factor_group_internal, map_tol);
    return;
  }

  //************************************************************
  const MasterSymGroup &Structure::factor_group() const {
    if(!factor_group_internal.size())
      generate_factor_group();
    return factor_group_internal;
  }

  //************************************************************
  const SymGroup &Structure::point_group() const {
    return factor_group().point_group();
  }

  //***********************************************************

  SymGroupRep const *Structure::permutation_symrep() const {
    if(perm_rep_ID == Index(-1))
      generate_permutation_representation();

    SymGroupRep const *perm_group(factor_group().representation(perm_rep_ID));
    if(!perm_group) {
      generate_permutation_representation();
      perm_group = factor_group().representation(perm_rep_ID);
    }

    return perm_group;
  }

  //***********************************************************

  Index Structure::permutation_symrep_ID() const {
    if(perm_rep_ID == Index(-1))
      generate_permutation_representation();

    return perm_rep_ID;
  }

  //***********************************************************

  SymGroupRep const *Structure::basis_permutation_symrep() const {
    if(basis_perm_rep_ID == Index(-1))
      generate_basis_permutation_representation();

    SymGroupRep const *perm_group(factor_group().representation(basis_perm_rep_ID));
    if(!perm_group) {
      generate_basis_permutation_representation();
      perm_group = factor_group().representation(basis_perm_rep_ID);
    }

    return perm_group;
  }

  //***********************************************************

  Index Structure::basis_permutation_symrep_ID() const {
    if(basis_perm_rep_ID == Index(-1))
      generate_basis_permutation_representation();

    return basis_perm_rep_ID;
  }

  //************************************************************

  /// Returns an Array of each *possible* Specie in this Structure
  Array<Specie> Structure::get_struc_specie() const {

    Array<Molecule> struc_molecule = get_struc_molecule();
    Array<Specie> struc_specie;

    Index i, j;

    //For each molecule type
    for(i = 0; i < struc_molecule.size(); i++) {
      // For each atomposition in the molecule
      for(j = 0; j < struc_molecule[i].size(); j++) {
        if(!struc_specie.contains(struc_molecule[i][j].specie)) {
          struc_specie.push_back(struc_molecule[i][j].specie);
        }
      }
    }

    return struc_specie;
  }

  //************************************************************

  /// Returns an Array of each *possible* Molecule in this Structure
  Array<Molecule> Structure::get_struc_molecule() const {

    //Check if basis is empty
    if(basis.size() == 0) {
      std::cerr << "Warning in Structure::get_struc_molecule():  basis.size() == 0" << std::endl;
    }

    Array<Molecule> struc_molecule;
    Index i, j;

    //loop over all Sites in basis
    for(i = 0; i < basis.size(); i++) {
      //loop over all Molecules in Site
      for(j = 0; j < basis[i].site_occupant().size(); j++) {
        //Collect unique Molecules
        if(!struc_molecule.contains(basis[i].site_occupant()[j])) {
          struc_molecule.push_back(basis[i].site_occupant()[j]);
        }
      }
    }//end loop over all Sites

    return struc_molecule;
  }

  //************************************************************

  /// Returns a list of how many of each specie exist in this Structure
  ///   The Specie types are ordered according to get_struc_specie()
  Array<int> Structure::get_num_each_specie() const {

    Array<Specie> struc_specie = get_struc_specie();
    Array<int> num_each_specie(struc_specie.size(), 0);

    Index i, j;
    // For each site
    for(i = 0; i < basis.size(); i++) {
      // For each atomposition in the molecule on the site
      for(j = 0; j < basis[i].occ().size(); j++) {
        // Count the present specie
        num_each_specie[ struc_specie.find(basis[i].occ()[j].specie)]++;
      }
    }

    return num_each_specie;
  }

  //************************************************************

  /// Returns a list of how many of each molecule exist in this Structure
  ///   The molecule types are ordered according to get_struc_molecule()
  Array<int> Structure::get_num_each_molecule() const {

    Array<Molecule> struc_molecule = get_struc_molecule();
    Array<int> num_each_molecule(struc_molecule.size(), 0);

    Index i;
    // For each site
    for(i = 0; i < basis.size(); i++) {
      // Count the molecule
      num_each_molecule[ struc_molecule.find(basis[i].occ())]++;
    }

    return num_each_molecule;
  }




  //************************************************************
  void Structure::fg_converge(double small_tol, double large_tol, double increment) {
    BasicStructure<Site>::fg_converge(factor_group_internal, small_tol, large_tol, increment);
    return;
  }

  //************************************************************
  void Structure::fg_converge(double large_tol) {
    BasicStructure<Site>::fg_converge(factor_group_internal, TOL, large_tol, (large_tol - TOL) / 10.0);
    return;
  }

  //************************************************************
  void Structure::print_factor_group(std::ostream &stream) const {
    stream << "Factor Group of " << title << ", containing "
           << factor_group().size() << " symmetry operations:\n";
    for(Index i = 0; i < factor_group_internal.size(); i++) {
      factor_group_internal[i].print(stream);
    }

    return;
  }

  //************************************************************
  void Structure::print5(std::ostream &stream, COORD_TYPE mode, int Va_mode, char term, int prec, int pad) const {
    std::string mol_name;
    std::ostringstream num_mol_list, coord_stream;
    Array<Site> vacancies;

    // this statement assumes that the scaling is 1.0 always, this may needed to be changed
    stream << title << std::endl;
    lattice().print(stream);

    //declare hash
    std::map<std::string, Array<Site> > siteHash;
    //declare iterator for hash
    std::map<std::string, Array<Site> >::iterator it;
    //loop through all sites
    for(Index i = 0; i < basis.size(); i++) {
      Site tsite = basis[i];
      mol_name = tsite.occ_name();

      if(mol_name != "Va") {
        //check if mol_name is already in the hash
        it = siteHash.find(mol_name);
        if(it != siteHash.end()) {
          Array<Site> tarray = it-> second;
          tarray.push_back(tsite);
          siteHash[mol_name]  = tarray;
        }
        // otherwise add a new pair
        else {
          Array<Site> tarray;
          tarray.push_back(tsite);
          siteHash[mol_name] = tarray;
        }
      }
      //store vacancies into a separate array
      else {
        vacancies.push_back(tsite);
      }
    }
    //print names of molecules and numbers, also populate coord_stream
    it = siteHash.begin();
    stream << it -> first;
    num_mol_list << it -> second.size();
    for(Index i = 0; i < it->second.size(); i++) {
      Site tsite = it->second.at(i);
      tsite.Coordinate::print(coord_stream, mode, term, prec, pad);
      coord_stream << std::endl;
    }
    it++;

    for(; it != siteHash.end(); it++) {
      stream << ' ' << it-> first;
      num_mol_list << ' ' << it-> second.size();
      for(Index i = 0; i < it->second.size(); i++) {
        Site tsite = it->second.at(i);
        tsite.Coordinate::print(coord_stream, mode, term, prec, pad);
        coord_stream << std::endl;
      }
    }
    // add vacancies depending on the mode
    if(Va_mode == 2)
      stream << " Va";
    if(Va_mode != 0) {
      for(Index i = 0; i < vacancies.size(); i++) {
        Site tsite = vacancies.at(i);
        tsite.Coordinate::print(coord_stream, mode, term, prec, pad);
        coord_stream << std::endl;
      }
    }
    stream << std::endl;
    stream << num_mol_list.str() << std::endl;
    //print the COORD_TYPE
    if(mode == FRAC)
      stream << "Direct\n";
    else if(mode == CART)
      stream << "Cartesian\n";
    else
      std::cerr << "error the mode isn't defined";
    stream << coord_stream.str() << std::endl;
    return;

  }
  //************************************************************
  //read a POSCAR like file and collect all the structure variables
  //modified to read PRIM file and determine which basis to use
  //Changed by Ivy to read new VASP POSCAR format
  void Structure::read(std::istream &stream) {
    int i, t_int;
    char ch;
    Array<double> num_elem;
    Array<std::string> elem_array;
    bool read_elem = false;
    std::string tstr;
    std::stringstream tstrstream;

    Site tsite(lattice());

    SD_flag = false;
    getline(stream, title);

    m_lattice.read(stream);

    stream.ignore(100, '\n');

    //Search for Element Names
    ch = stream.peek();
    while(ch != '\n' && !stream.eof()) {
      if(isalpha(ch)) {
        read_elem = true;
        stream >> tstr;
        elem_array.push_back(tstr);
        ch = stream.peek();
      }
      else if(ch == ' ' || ch == '\t') {
        stream.ignore();
        ch = stream.peek();
      }
      else if(ch >= '0' && ch <= '9') {
        break;
      }
      else {
        throw std::runtime_error(
          std::string("Error attempting to read Structure. Error reading atom names."));
      }
    }

    if(read_elem == true) {
      stream.ignore(10, '\n');
      ch = stream.peek();
    }

    //Figure out how many species
    int num_sites = 0;
    while(ch != '\n' && !stream.eof()) {
      if(ch >= '0' && ch <= '9') {
        stream >> t_int;
        num_elem.push_back(t_int);
        num_sites += t_int;
        ch = stream.peek();
      }
      else if(ch == ' ' || ch == '\t') {
        stream.ignore();
        ch = stream.peek();
      }
      else {
        std::cerr << "Error in line 6 of structure input file. Line 6 of structure input file should contain the number of sites." << std::endl;
        exit(1);
      }
    }
    stream.get(ch);

    // fractional coordinates or cartesian
    COORD_MODE input_mode(FRAC);

    stream.get(ch);
    while(ch == ' ' || ch == '\t') {
      stream.get(ch);
    }

    if(ch == 'S' || ch == 's') {
      SD_flag = true;
      stream.ignore(1000, '\n');
      while(ch == ' ' || ch == '\t') {
        stream.get(ch);
      }
      stream.get(ch);
    }

    if(ch == 'D' || ch == 'd') {
      input_mode.set(FRAC);
    }
    else if(ch == 'C' || ch == 'c') {
      input_mode.set(CART);
    }
    else if(!SD_flag) {
      std::cerr << "Error in line 7 of structure input file. Line 7 of structure input file should specify Direct, Cartesian, or Selective Dynamics." << std::endl;
      exit(1);
    }
    else if(SD_flag) {
      std::cerr << "Error in line 8 of structure input file. Line 8 of structure input file should specify Direct or Cartesian when Selective Dynamics is on." << std::endl;
      exit(1);
    }

    stream.ignore(1000, '\n');
    //Clear basis if it is not empty
    if(basis.size() != 0) {
      std::cerr << "The structure is going to be overwritten." << std::endl;
      basis.clear();
    }

    if(read_elem) {
      int j = -1;
      int sum_elem = 0;
      basis.reserve(num_sites);
      for(i = 0; i < num_sites; i++) {
        if(i == sum_elem) {
          j++;
          sum_elem += num_elem[j];
        }

        tsite.read(stream, elem_array[j], SD_flag);
        basis.push_back(tsite);
      }
    }
    else {
      //read the site info
      basis.reserve(num_sites);
      for(i = 0; i < num_sites; i++) {
        tsite.read(stream, SD_flag);
        if((stream.rdstate() & std::ifstream::failbit) != 0) {
          std::cerr << "Error reading site " << i + 1 << " from structure input file." << std::endl;
          exit(1);
        }
        basis.push_back(tsite);
      }
    }

    // Check whether there are additional sites listed in the input file
    Vector3< double > coord;
    stream >> coord;
    if((stream.rdstate() & std::ifstream::failbit) == 0) {
      std::cerr << "ERROR: too many sites listed in structure input file." << std::endl;
      exit(1);
    }

    update();
    return;

  }

  //************************************************************
  // print structure and include all possible occupants on each site, using VASP5 format - NOT ALLOWED

  //void Structure::print5(std::ostream &stream, COORD_TYPE mode)
  //{
  //    main_print( stream, mode, true, 0);
  //}

  //************************************************************
  // print structure and include all possible occupants on each site, using VASP4 format

  void Structure::print(std::ostream &stream, COORD_TYPE mode) const {
    main_print(stream, mode, false, 0);
  }

  //************************************************************
  // print structure and include current occupant on each site, using VASP5 format

  void Structure::print5_occ(std::ostream &stream, COORD_TYPE mode) const {
    main_print(stream, mode, true, 1);
  }

  //************************************************************
  // print structure and include current occupant on each site, using VASP4 format

  void Structure::print_occ(std::ostream &stream, COORD_TYPE mode) const {
    main_print(stream, mode, false, 1);
  }

  //************************************************************
  // Private print routine called by public routines
  //   by BP, collected and modified the existing print routines (by John G?) into 1 function
  void Structure::main_print(std::ostream &stream, COORD_TYPE mode, bool version5, int option) const {
    //std::cout << "begin Structure::main_print()" << std::endl;
    // No Sorting (For now... Figure out how to do this for molecules later...)
    // If option == 0 (print all possible occupants), make sure comparing all possible occupants
    // If option == 1 (print occupying molecule name), compare just the occupant
    // If option == 2 (print all atoms of molecule), (don't do this yet)

    if(option < 0 || option > 2) {
      std::cerr << "Error in Structure::main_print()." << std::endl;
      std::cerr << "  option " << option << " does not exist.  Use option = 0 or 1" << std::endl;
      std::exit(1);
    }
    else {
      if(version5 && (option == 0)) {
        std::cerr << "Error in Structure::main_print()." << std::endl;
        std::cerr << "  Trying to print a Structure with VASP version 5 format" << std::endl;
        std::cerr << "  and option == 0 (print all possible occupants).  This can't be done." << std::endl;
        std::cerr << "  Either use version 4 format, or option 1 (print occupying molecule name)." << std::endl;
        std::exit(1);
      }

      if(option == 2) {
        std::cerr << "Error in Structure::main_print()." << std::endl;
        std::cerr << "  Trying to print all atom positions (option 2), but this is not yet coded." << std::endl;
        std::exit(1);
      }
    }

    Array<std::string> site_names;

    // This is for sorting molecules by type. - NO LONGER USING THIS
    //Array<int> site_order;
    //site_order.reserve(basis.size());

    // Count up each species and their names
    // This is total for structure - NOT GOING TO SET THIS ANYMORE
    // num_each_specie.resize(struc_molecule.size(), 0);

    // This is consequentive molecules of same name
    // If option == 0 (print all possible occupants), make sure comparing all possible occupants
    // If option == 1 (print occupying molecule name), compare just the occupant
    // If option == 2 (print all atoms of molecule), (don't do this yet)
    Array<int> num_each_specie_for_printing;

    // if option == 1 (print current occupants), check that current state is not -1 (unknown occupant)
    if(option == 1) {
      //std::cout << "  check curr state" << std::endl;
      for(Index j = 0; j < basis.size(); j++) {
        if(basis[j].site_occupant().value() == -1) {
          std::cerr << "Error in Structure::main_print() using option 1 (print occupying molecule name)." << std::endl;
          std::cerr << "  basis " << j << " occupant state is unknown." << std::endl;
          std::exit(1);
        }
      }
    }


    //std::cout << "  get num each specie for printing" << std::endl;

    for(Index i = 0; i < basis.size(); i++) {
      if(option == 0) { //(print all possible occupants)
        if(i == 0)
          num_each_specie_for_printing.push_back(1);
        else if(basis[i - 1].site_occupant().compare(basis[i].site_occupant(), false))
          num_each_specie_for_printing.back()++;
        else
          num_each_specie_for_printing.push_back(1);
      }
      else if(option == 1) {   //(print all occupying molecule)
        if(basis[i].occ_name() == "Va") {
          continue;
        }

        if(i == 0) {
          num_each_specie_for_printing.push_back(1);
          site_names.push_back(basis[i].occ_name());
        }
        else if(basis[i - 1].occ_name() == basis[i].occ_name())
          num_each_specie_for_printing.back()++;
        else {
          num_each_specie_for_printing.push_back(1);
          site_names.push_back(basis[i].occ_name());
        }

      }
    }

    stream << title << '\n';

    // Output lattice: scaling factor and lattice vectors
    //std::cout << "  print lattice" << std::endl;
    lattice().print(stream);

    // Output species names
    //std::cout << "  print specie names" << std::endl;
    if(version5) {
      for(Index i = 0; i < site_names.size(); i++) {
        stream << " " << site_names[i];
      }
      stream << std::endl;
    }


    // Output species counts
    //std::cout << "  print specie counts" << std::endl;
    for(Index i = 0; i < num_each_specie_for_printing.size(); i++) {
      stream << " " << num_each_specie_for_printing[i];
    }
    stream << std::endl;

    if(SD_flag) {
      stream << "Selective Dynamics\n";
    }


    COORD_MODE output_mode(mode);

    stream << COORD_MODE::NAME() << '\n';

    // Output coordinates
    //std::cout << "  print coords" << std::endl;

    for(Index i = 0; i < basis.size(); i++) {
      if(option == 0) {	// print all possible occupying molecules
        basis[i].print(stream);
        stream << '\n';
      }
      else if(option == 1 && basis[i].occ_name() != "Va") {	// print occupying molecule
        basis[i].print_occ(stream);
        stream << '\n';
      }
      else if(option == 2)	// print all atoms in molecule
        basis[i].print_mol(stream, 0, '\n', SD_flag);
    }
    stream << std::flush ;

    //std::cout << "finish Structure::main_print()" << std::endl;

    return;
  }

  //***********************************************************
  void Structure::print_xyz(std::ostream &stream) const {
    stream << basis.size() << '\n';
    stream << title << '\n';
    stream.precision(7);
    stream.width(11);
    stream.flags(std::ios::showpoint | std::ios::fixed | std::ios::right);

    for(Index i = 0; i < basis.size(); i++) {
      stream << std::setw(2) << basis[i].occ_name();
      stream << std::setw(12) << basis[i](CART) << '\n';
    }

  }

  //***********************************************************

  void Structure::print_cif(std::ostream &stream) {
    const char quote = '\'';
    const char indent[] = "   ";

    //double amag, bmag, cmag;
    //double alpha, beta, gamma;

    // Copying format based on VESTA .cif output.

    // Heading text.

    stream << '#';
    for(int i = 0; i < 70; i++) {
      stream << '=';
    }
    stream << "\n\n";
    stream << "# CRYSTAL DATA\n\n";
    stream << '#';
    for(int i = 0; i < 70; i++) {
      stream << '-';
    }
    stream << "\n\n";
    stream << "data_CASM\n\n\n";

    stream.precision(5);
    stream.width(11);
    stream.flags(std::ios::showpoint | std::ios::fixed | std::ios::left);

    stream << std::setw(40) << "_pd_phase_name" << quote << title << quote << '\n';
    stream << std::setw(40) << "_cell_length_a" << lattice().lengths[0] << '\n';
    stream << std::setw(40) << "_cell_length_b" << lattice().lengths[1] << '\n';
    stream << std::setw(40) << "_cell_length_c" << lattice().lengths[2] << '\n';
    stream << std::setw(40) << "_cell_angle_alpha" << lattice().angles[0] << '\n';
    stream << std::setw(40) << "_cell_angle_beta" << lattice().angles[1] << '\n';
    stream << std::setw(40) << "_cell_angle_gamma" << lattice().angles[2] << '\n';
    stream << std::setw(40) << "_symmetry_space_group_name_H-M" << quote << "TBD" << quote << '\n';
    stream << std::setw(40) << "_symmetry_Int_Tables_number" << "TBD" << "\n\n";

    stream << "loop_\n";
    stream << "_symmetry_equiv_pos_as_xyz\n";

    // Equivalent atom positions here. Form: 'x, y, z', '-x, -y, -z', 'x+1/2, y+1/2, z', etc.
    // Use stream << indent << etc.

    stream << '\n';
    stream << "loop_\n";
    stream << indent << "_atom_site_label" << '\n';
    stream << indent << "_atom_site_occupancy" << '\n';
    stream << indent << "_atom_site_fract_x" << '\n';
    stream << indent << "_atom_site_fract_y" << '\n';
    stream << indent << "_atom_site_fract_z" << '\n';
    stream << indent << "_atom_site_adp_type" << '\n';
    stream << indent << "_atom_site_B_iso_or_equiv" << '\n';
    stream << indent << "_atom_site_type_symbol" << '\n';

    // Use stream << indent << etc.
  }


  //***********************************************************
  /**
  */
  //***********************************************************
  /*bool Structure::read_species() {

    Array<std::string> names;
    Array<double> masses;
    Array<double> magmoms;
    Array<double> Us;
    Array<double> Js;
    std::string tstring;
    double value;
    Index i;
    bool match = true;
    std::ifstream stream;
    stream.open("SPECIES");

    Array<Specie> struc_species = get_struc_specie();

    //If cannot open species file, will assign default masses
    //read in from elements file.
    if(!stream) {
      std::cout << "*************************************\n"
                << "ERROR in Structure::read_species: \n"
                << "Could not open SPECIES file. \n"
                << "Going to assign default masses. \n"
                << "*************************************\n";
      for(i = 0; i < struc_species.size(); i++) {
        masses.push_back(Elements::get_mass(struc_species[i].name));
        names.push_back(struc_species[i].name);
      }

      assign_species(names, masses, magmoms, Us, Js);
      return false;
    }

    //Reading in from SPECIES
    for(i = 0; i < struc_species.size(); i++) {
      stream >> tstring;
      names.push_back(tstring);
    }

    //Check to see the elements read in actually match
    for(i = 0; i < names.size(); i++) {

      Specie tspecie(names[i]);

      if(struc_species.find(tspecie) == struc_species.size()) {
        std::cout << "****************************************\n"
                  << "ERROR in Structure::read_species: \n"
                  << "None of species in SPECIES match those\n"
                  << "belonging to this structure.\n"
                  << "Going to assign default masses.\n"
                  << "****************************************\n";
        match = false;
        break;
      }
    }

    //If could not find elements of Structure in SPECIES,
    //will look it up in default element list.
    if(match == false) {


      //This is so that the incorrect species names that were read in
      //won't be used in assign_species
      //names.clear();

      for(i = 0; i < struc_species.size(); i++) {
        masses.push_back(Elements::get_mass(struc_species[i].name));
        names.push_back(struc_species[i].name);
      }

      assign_species(names, masses, magmoms, Us, Js);
      return false;
    }

    while(stream >> tstring) {

      if((tstring == "mass") || (tstring == "MASS")) {

        for(i = 0; i < struc_species.size(); i++) {
          stream >> value;
          masses.push_back(value);
        }
        stream.ignore(256, '\n');
      }
      else if((tstring == "magmom") || (tstring == "MAGMOM")) {

        for(i = 0; i < struc_species.size(); i++) {
          stream >> value;
          magmoms.push_back(value);
        }
        stream.ignore(256, '\n');
      }
      else if((tstring == "u") || (tstring == "U")) {

        for(i = 0; i < struc_species.size(); i++) {
          stream >> value;
          Us.push_back(value);

        }
        stream.ignore(256, '\n');
      }
      else if((tstring == "j") || (tstring == "J")) {

        for(i = 0; i < struc_species.size(); i++) {
          stream >> value;
          Js.push_back(value);

        }
        stream.ignore(256, '\n');
      }
      else {
        std::cout << "***********************************\n"
                  << "ERROR in Structure::read_species\n: "
                  << stream.rdbuf()
                  << " is not a valid field for SPECIES \n"
                  << "***********************************\n";
      }
    }

    stream.close();

    assign_species(names, masses, magmoms, Us, Js);

    return true;
  };
  */
  //***********************************************************
  /**
   * Assigns the names, masses, magmoms, Us, and Js read in
   * from SPECIES to the right atom.
   */
  //***********************************************************

  // THIS NEEDS TO BE MOVED TO Site/Molecule
  /*
  void Structure::assign_species(Array<std::string> &names, Array<double> &masses, Array<double> &magmoms, Array<double> &Us, Array<double> &Js) {

    Index j;

    for(Index b = 0; b < basis.size(); b++) {

      j = names.find(basis[b].site_occupant()[0][0].specie.name);

      if(j == names.size()) {
        std::cerr << "****************************************\n"
                  << "ERROR in Structure::assign_species:\n"
                  << "Could not find "
                  << basis[b].site_occupant()[0][0].specie.name
                  << "\n"
                  << "****************************************\n";
      }

      if(masses.size() != 0) {

        basis[b].site_occupant()[0][0].specie.mass = masses[j];

      }

      if(magmoms.size() != 0) {

        basis[b].site_occupant[0][0].specie.magmom = magmoms[j];

      }

      if(Us.size() != 0) {

        basis[b].site_occupant[0][0].specie.U = Us[j];

      }

      if(Js.size() != 0) {

        basis[b].site_occupant[0][0].specie.J = Js[j];

      }
    }


  };
  */

  //***********************************************************
  /**
   * It is NOT wise to use this function unless you have already
   * initialized a superstructure with lattice vectors.
   *
   * It is more wise to use the two methods that call this method:
   * Either the overloaded * operator which does:
   *  SCEL_Lattice * Prim_Structrue = New_Superstructure
   *       --- or ---
   *  New_Superstructure=Prim_Structure.create_superstruc(SCEL_Lattice);
   *
   *  Both of these will return NEW superstructures.
   */
  //***********************************************************

  void Structure::fill_supercell(const Structure &prim, double map_tol) {
    Index i, j;

    //Updating lattice_trans matrices that connect the supercell to primitive -- Changed 11/08/12
    m_lattice.calc_conversions();
    SymGroup latvec_pg;
    m_lattice.generate_point_group(latvec_pg);

    SD_flag = prim.SD_flag;
    PrimGrid prim_grid(prim.lattice(), lattice());

    basis.clear();
    Coordinate tcoord(lattice());

    //loop over basis sites of prim
    for(j = 0; j < prim.basis.size(); j++) {

      //loop over prim_grid points
      for(i = 0; i < prim_grid.size(); i++) {

        //push back translated basis site of prim onto superstructure basis
        basis.push_back(prim.basis[j] + prim_grid.coord(i, PRIM));

        //reset lattice for most recent superstructure Site
        //set_lattice() converts fractional coordinates to be compatible with new lattice
        basis.back().set_lattice(lattice());

        basis.back().within();
        for(Index k = 0; k < basis.size() - 1; k++) {
          if(basis[k].compare(basis.back(), map_tol)) {
            basis.pop_back();
            break;
          }
        }
      }
    }
    //std::cout << "WORKING ON FACTOR GROUP " << &factor_group_internal << " for structure with volume " << prim_grid.size() << ":\n";
    //trans_and_expand primitive factor_group
    for(i = 0; i < prim.factor_group().size(); i++) {
      if(latvec_pg.find_no_trans(prim.factor_group()[i]) == latvec_pg.size()) {
        continue;
      }
      else {
        for(Index j = 0; j < prim_grid.size(); j++) {
          factor_group_internal.push_back(SymOp(prim.factor_group()[i].get_matrix(CART),
                                                prim.factor_group()[i].tau(CART) + prim_grid.coord(j, SCEL)(CART), lattice(), CART));
          factor_group_internal.back().within();
          //OLD VERSION:
          //factor_group_internal.push_back(SymOp(prim_grid.coord(j, SCEL))*prim.factor_group()[i]);
          //factor_group_internal.back().set_lattice(lattice, CART);
        }
      }
    }
    if(factor_group_internal.size() > 200) {// how big is too big? this is at least big enough for FCC conventional cell
#ifndef NDEBUG
      std::cerr << "WARNING: You have a very large factor group of a non-primitive structure. Certain symmetry features will be unavailable.\n";
#endif
      factor_group_internal.invalidate_multi_tables();
    }
    //std::cout << "Final size is: " << factor_group_internal.size() << "\n";
    update();

    return;
  }

  //***********************************************************
  /**
   * Operates on the primitive structure and takes as an argument
   * the supercell lattice.  It then returns a new superstructure.
   *
   * This is similar to the Lattice*Primitive routine which returns a
   * new superstructure.  Unlike the fill_supercell routine which takes
   * the primitive structure, this WILL fill the sites.
   */
  //***********************************************************

  Structure Structure::create_superstruc(const Lattice &scel_lat, double map_tol) const {
    Structure tsuper(scel_lat);
    tsuper.fill_supercell(*this);
    return tsuper;
  }

  //***********************************************************
  /*shorttag allows the user to decide whether or not to print the
   * entire symmetry matrix.  If shorttag=0, then only the name of the
   * operation and the eigenvector will be printed.  If it equals anything else,
   * then the symmetry operation matrix will be printed out.
   */

  void Structure::print_site_symmetry(std::ostream &stream, COORD_TYPE mode, int shorttag = 1) {
    GenericOrbitBranch<SiteCluster> asym_unit(lattice());
    asym_unit.generate_asymmetric_unit(basis, factor_group());

    stream <<  " Printing symmetry operations that leave each site unchanged:\n \n";
    for(Index i = 0; i < asym_unit.size(); i++) {
      for(Index j = 0; j < asym_unit[i].size(); j++) {
        stream << "Site: ";
        asym_unit.at(i).at(j).at(0).print(stream); //Print the site (not cluster) for the asymmetric unit
        stream << " Total Symmetry Operations: " << asym_unit[i][j].clust_group.size() << "\n";

        for(Index nc = 0; nc < asym_unit[i][j].clust_group.size(); nc++) {
          //print_short is a new fxn I added in SymOp to not print out the whole symmetry matrix
          if(shorttag == 0) {
            stream <<  std::setw(4)  << nc + 1 << ": ";
            //asym_unit[i][j].clust_group[nc].print_short(stream); // I turned this off because the "print_short" function does not appear to exist...
          }


          else {
            stream.flags(std::ios::left);
            stream << "\n" <<  std::setw(3)  << nc + 1 << ": ";
            stream.unsetf(std::ios::left);
            asym_unit[i][j].clust_group[nc].print(stream);
          }
        }
        stream << "\n  ---------------------------------------------------------------------   \n\n";
      }
    }
    return;
  }

  //***********************************************************

  void Structure::reset() {
    //std::cout << "begin reset() " << this << std::endl;

    for(Index nb = 0; nb < basis.size(); nb++) {
      basis[nb].set_basis_ind(nb);
    }
    within();
    factor_group_internal.clear();

    /** Should we also invalidate the occupants?
    for(Index i = 0; i < basis.size(); i++) {
      basis[i].site_occupant.set_value(-1);
    }
    */
    //std::cout << "finish reset()" << std::endl;
    return;
  }

  //***********************************************************
  /**
   * A flowertree has clusters of every size organized like the
   * branches of a regular orbitree, but every cluster in the
   * entire flowertree has the same pivot.
   */
  //***********************************************************
  void Structure::generate_flowertrees(const SiteOrbitree &in_tree, Array<SiteOrbitree> &out_trees) {
    if(out_trees.size() != 0) {
      std::cerr << "WARNING in Structure::generate_flowertrees_safe" << std::endl;
      std::cerr << "The provided array of SiteOrbitree wasn't empty! Hope nothing important was there..." << std::endl;
      out_trees.clear();
    }


    //Theres a flowertree for each basis site b
    for(Index b = 0; b < basis.size(); b++) {
      SiteOrbitree ttree(lattice());    //Weird behavior without this
      ttree.reserve(in_tree.size());
      out_trees.push_back(ttree);
      //We want clusters of every size in the tree, except the 0th term (...right guys?)
      for(Index s = 1; s < in_tree.size(); s++) {
        //Make each site basis into a cluster so we can use extract_orbits_including
        SiteCluster tsiteclust(lattice());
        tsiteclust.within();
        tsiteclust.push_back(basis[b]);
        //Make a branch where all the clusters of a given size s go
        SiteOrbitBranch tbranch(lattice());
        //Push back flower onto tree
        out_trees[b].push_back(tbranch);
        //Get one flower for a given basis site
        in_tree[s].extract_orbits_including(tsiteclust, out_trees[b].back());   //couldn't get it to work withough directly passing tree.back() (i.e. can't make branch then push_back)
      }
      out_trees[b].get_index();
      out_trees[b].collect_basis_info(*this);
    }

    return;
  }

  //***********************************************************
  // Fills an orbitree such that each OrbitBranch corresponds to a basis site and contains all orbits
  // that include that basis site
  void Structure::generate_basis_bouquet(const SiteOrbitree &in_tree, SiteOrbitree &out_tree, Index num_sites) {
    Index na, np, nb, ne;

    out_tree.clear(); //Added by Ivy 11/07/12

    //make room in out_tree for flowers
    //out_tree.reserve(basis.size());

    out_tree.lattice = in_tree.lattice;
    //out_tree.set_lattice(in_tree.lattice,FRAC);   //better?

    //get asymmetric unit
    GenericOrbitBranch<SiteCluster> asym_unit(lattice());
    asym_unit.generate_asymmetric_unit(basis, factor_group());


    for(na = 0; na < asym_unit.size(); na++) {
      asym_unit.prototype(na).clust_group.get_character_table();
      //std::cout << "Working on asym_unit element " << na + 1 << '\n';
      nb = out_tree.size();

      //Add new branch with asym_unit.prototype(na) as pivot
      out_tree.push_back(SiteOrbitBranch(asym_unit.prototype(na)));

      for(np = 0; np < in_tree.size(); np++) {
        if(num_sites != in_tree[np].num_sites()) {
          continue;
        }
        //std::cout << "Starting to extract petals from OrbitBranch " << np << '\n';
        in_tree[np].extract_orbits_including(asym_unit.prototype(na), out_tree.back());
      }


      for(ne = 1; ne < asym_unit[na].equivalence_map.size(); ne++) {
        out_tree.push_back(out_tree[nb]);
        out_tree.back().apply_sym(asym_unit[na].equivalence_map[ne][0]);
      }
    }
    //out_tree.get_index(); //Should not get_index because we want to keep the in_tree indices Ivy 01/15/14
    out_tree.collect_basis_info(*this);
    return;
  }

  //***********************************************************
  /**
   * Fills out_tree with OrbitBranches with pivots corresponding
   * to the different asym_unit sites.  The orbits of each branch
   * are "petals" radiating from that branch's pivot.
   *
   * @param in_tree General tree that holds clusters of all sizes
   * @param out_tree Holds all flowers (orbits) radiating from asym_unit sites
   * @param num_sites Size of clusters filling up out_tree
   */
  //***********************************************************
  // Added by Ivy 10/17/12
  void Structure::generate_asym_bouquet(const SiteOrbitree &in_tree, SiteOrbitree &out_tree, Index num_sites) {
    Index na, np;//, nb;

    out_tree.clear();

    //get asymmetric unit
    GenericOrbitBranch<SiteCluster> asym_unit(lattice());
    asym_unit.generate_asymmetric_unit(basis, factor_group());

    //make room in out_tree for flowers
    out_tree.reserve(asym_unit.size());

    out_tree.lattice = in_tree.lattice;


    for(na = 0; na < asym_unit.size(); na++) {

      //nb = out_tree.size();

      //Add new branch with asym_unit.prototype(na) as pivot
      out_tree.push_back(SiteOrbitBranch(asym_unit.prototype(na)));

      for(np = 0; np < in_tree.size(); np++) {
        if(num_sites != in_tree[np].num_sites()) {
          continue;
        }

        //prototype of na orbit
        in_tree[np].extract_orbits_including(asym_unit.prototype(na), out_tree.back());
      }

    }
    //out_tree.get_index(); //Should not get_index because we want to keep the in_tree indices Ivy 01/15/14
    out_tree.collect_basis_info(*this);
    return;


  }

  //John G 230913

  //***********************************************************
  /**
   * A flowertree is NOT a bouquet.
   * Bouquets are constructed from an orbitree (generate_basis_bouquet)
   * by taking every cluster of a given size and storing them
   * in branches, where each branch contains clusters with a
   * common pivot.
   * A flowertree has clusters of every size organized like the
   * branches of a regular orbitree, but every cluster in the
   * entire flowertree has the same pivot.
   *
   * Begin by making n bouquets, where n is the size of the
   * larges cluster in the provided orbitree. Then shuffle
   * all the bouquets together and sort them by basis site.
   * The result is an array of "flowertrees", which have been
   * made by picking out flowers of every bouquet that have
   * the same pivot. You end up with one flowertree per basis site
   * each of which has clusters from size 1 to n.
   */
  //***********************************************************
  void Structure::generate_flowertrees_safe(const SiteOrbitree &in_tree, Array<SiteOrbitree> &out_trees) {  //can we make it const? It would require generate_basis_bouquet to also be const
    if(out_trees.size() != 0) {
      std::cerr << "WARNING in Structure::generate_flowertrees_safe" << std::endl;
      std::cerr << "The provided array of SiteOrbitree wasn't empty! Hope nothing important was there..." << std::endl;
      out_trees.clear();
    }

    //Determine what the largest cluster in the given orbitree is, accounting for the fact that there's a 0 point cluster
    Index max_clust_size = in_tree.size() - 1;
    //Plant all the bouquets, one for each cluster size and store them in
    Array<SiteOrbitree> bouquets;

    for(Index i = 1; i < (max_clust_size + 1); i++) {
      SiteOrbitree tbouquet(lattice());
      generate_basis_bouquet(in_tree, tbouquet, i);
      bouquets.push_back(tbouquet);
    }

    //Pluck a branch off each bouquet for a given basis site and stuff it into a SiteOribitree (this is a flowertree)
    //SiteOrbitree tbouquet=in_tree;   //This is so I don't have to worry about initializing things I'm scared of
    SiteOrbitree tbouquet(lattice());

    //How many flowertrees do we need? As many as there are basis sites, i.e. bouquet[i].size();
    for(Index i = 0; i < basis.size(); i++) {
      tbouquet.clear();
      //How many trees, i.e. cluster sizes
      for(Index j = 0; j < bouquets.size(); j++) {
        //Not pushing back empty cluster!!
        tbouquet.push_back(bouquets[j][i]);
      }
      tbouquet.get_index();
      tbouquet.collect_basis_info(*this);
      out_trees.push_back(tbouquet);
    }

    return;
  }

  //\John G 230913


  //*********************************************************

  void Structure::map_superstruc_to_prim(Structure &prim) {

    Index prim_to_scel;
    Site shifted_site(prim.lattice());


    //Check that (*this) is actually a supercell of the prim
    if(!lattice().is_supercell_of(prim.lattice(), prim.point_group())) {
      std::cerr << "*******************************************\n"
                << "ERROR in Structure::map_superstruc_to_prim:\n"
                << "The structure \n";
      print(std::cerr);
      std::cerr << "is not a supercell of the given prim!\n";
      prim.print(std::cerr);
      std::cerr << "*******************************************\n";
      exit(1);
    }

    //Get prim grid of supercell to get the lattice translations
    //necessary to stamp the prim in the superstructure
    PrimGrid prim_grid(prim.lattice(), lattice());

    // Translate each of the prim atoms by prim_grid translation
    // vectors, and map that translated atom in the supercell.
    for(Index pg = 0; pg < prim_grid.size(); pg++) {
      for(Index pb = 0; pb < prim.basis.size(); pb++) {
        shifted_site = prim.basis[pb];
        shifted_site += prim_grid.coord(pg, PRIM);
        shifted_site.set_lattice(lattice(), CART);
        shifted_site.within();

        // invalidate asym_ind and basis_ind because when we use
        // Structure::find, we don't want a comparison using the
        // basis_ind and asym_ind; we want a comparison using the
        // cartesian and Specie type.

        shifted_site.set_basis_ind(-1);
        prim_to_scel = find(shifted_site);

        if(prim_to_scel == basis.size()) {
          std::cerr << "*******************************************\n"
                    << "ERROR in Structure::map_superstruc_to_prim:\n"
                    << "Cannot associate site \n"
                    << shifted_site << "\n"
                    << "with a site in the supercell basis. \n"
                    << "*******************************************\n";
          std::cerr << "The basis_ind is "
                    << shifted_site.basis_ind() << "\n ";
          exit(2);
        }

        // Set ind_to_prim of the basis site
        //basis[prim_to_scel].ind_to_prim = pb;
      }
    }
  };

  //***********************************************************
  //
  void Structure::set_lattice(const Lattice &new_lat, COORD_TYPE mode) {
    bool is_equiv(lattice() == new_lat);
    COORD_TYPE not_mode(CART);
    if(mode == CART) {
      not_mode = FRAC;
    }

    for(Index nb = 0; nb < basis.size(); nb++) {
      basis[nb].invalidate(not_mode);
    }

    m_lattice = new_lat;


    if(is_equiv)
      factor_group_internal.set_lattice(new_lat, mode);
    else
      reset();
  }

  //John G 051112
  //***********************************************************
  /**
   * Given a rotational matrix rotate the
   * ENTIRE structure in cartesian space. This yields the same
   * structure, just with different cartesian definitions of
   * the lattice vectors
   *
   * Non primitive structures aren't allowed because you can't
   * update the new primitive pointer without having the new
   * primitive structure you want it pointing at falling out of
   * scope.
   */
  //***********************************************************

  Structure Structure::reorient(const Matrix3<double> reorientmat, bool override) const {
    if(!is_primitive() && !override) {
      std::cerr << "ERROR in Structure::reorient" << std::endl;
      std::cerr << "This function is for primitive cells only. Reduce your structure and try again." << std::endl;
      exit(11);
    }

    Structure reorientstruc(*this);

    Lattice reorientlat(reorientmat * lattice().lat_column_mat());
    reorientstruc.set_lattice(reorientlat, FRAC);

    if(override) {
      std::cerr << "WARNING in Structure::reorient (are you using Structure::align_with or Structure::align_standard?)" << std::endl;
      std::cerr << "You've chosen to forcibly reorient or align a non-primitive structure. The primitive lattice of your rotated structure will point to itself." << std::endl;
    }

    reorientstruc.update();

    return reorientstruc;
  }


  //***********************************************************
  /**
   *	This function takes two structures and returns a third
   *	structure identical to the one it is called on, only
   *	its vectors are reoriented to match the the structure
   *	it is passed. I.e. the returned structure is reoriented
   *	to have a and axb pointing in the same direction as
   *	the structure it is passed on.
   *
   *	This function was meant to be used with Structure::stack_on,
   *	which effectively stacks different cells to create
   *	heterostructures
   */
  //***********************************************************

  Structure Structure::align_with(const Structure &refstruc, bool override) const {
    if(!is_primitive() && !override) {
      std::cerr << "ERROR in Structure::align_with" << std::endl;
      std::cerr << "This function is for primitive cells only. Reduce your structure and try again." << std::endl;
      exit(12);
    }

    Vector3<double> rotaxis;
    double angle;
    Matrix3<double> rotmat;

    //First align the a vectors
    rotaxis = lattice()[0].cross(refstruc.lattice()[0]);
    angle = lattice()[0].get_signed_angle(refstruc.lattice()[0], rotaxis);

    rotmat = rotaxis.get_rotation_mat(angle);

    Structure aaligned = reorient(rotmat, override);

    //Then turn axb along the a axis
    rotaxis = aaligned.lattice()[0];
    angle = (aaligned.lattice()[0].cross(aaligned.lattice()[1])).get_signed_angle(refstruc.lattice()[0].cross(refstruc.lattice()[1]), rotaxis);

    rotmat = rotaxis.get_rotation_mat(angle);
    return aaligned.reorient(rotmat, override);
  }

  //***********************************************************
  /**
   * Reorients structure to have a along (x,0,0), b along (x,y,0)
   * and c along (x,y,z).
   */
  //***********************************************************

  Structure Structure::align_standard(bool override) const {
    if(!is_primitive() && !override) {
      std::cerr << "ERROR in Structure::align_with_standard" << std::endl;
      std::cerr << "This function is for primitive cells only. Reduce your structure and try again." << std::endl;
      exit(15);
    }

    Lattice standardlat(Vector3<double>(1, 0, 0),
                        Vector3<double>(0, 1, 0),
                        Vector3<double>(0, 0, 1));

    Structure standardstruc(standardlat);

    return align_with(standardstruc, override);

  }

  //***********************************************************
  /**
   * Returns a heterostructre. The structure this function is
   * called onto will be stretched to match a and b of the
   * structure is it passed (understruc). The returned strucure
   * will contain the basis of both structures, stacked on top
   * of each other in the c direction.
   *
   * This function only matches the ab planes.
   */
  //***********************************************************

  Structure Structure::stack_on(const Structure &understruc, bool override) const {
    Structure overstruc(*this);

    //Before doing anything check to see that lattices are aligned correctly (paralled ab planes)
    if(!override) {
      double axbangle;
      axbangle = (understruc.lattice()[0].cross(understruc.lattice()[1])).get_angle(overstruc.lattice()[0].cross(overstruc.lattice()[1]));

      if(!almost_zero(axbangle)) {
        std::cerr << "ERROR in Structure::stack_on" << std::endl;
        std::cerr << "Lattice mismatch! Your structures are oriented differently in space. ab planes of structures must be parallel before stacking.\
                    Redefine your structure or use Structure::align_with to fix issue." << std::endl;
        std::cerr << "Your vectors were:\n" << understruc.lattice()[0].cross(understruc.lattice()[1]) << "\n" << overstruc.lattice()[0].cross(overstruc.lattice()[1]) << std::endl;
        exit(13);
      }

      //Also check if c axes are on the same side of abplane
      //else if
      //{
      //}
    }

    else {
      std::cerr << "WARNING in Structure::stack_on" << std::endl;
      std::cerr << "You've chosen to ignore if ab planes of your structures are not parallel and/or c axes point the right way.\
                This will almost surely result in a malformed structure. Do not take the output for granted." << std::endl;
    }


    //First stretch a and b of overstruc to match a and b of understruc
    Lattice newoverstruclat(understruc.lattice()[0],
                            understruc.lattice()[1],
                            overstruc.lattice()[2]);

    overstruc.set_lattice(newoverstruclat, FRAC);


    //Create a lattice big enough to fit both structures
    Lattice heterolat(understruc.lattice()[0], //=overstruc.lattice()[0]
                      understruc.lattice()[1], //=overstruc.lattice()[1]
                      understruc.lattice()[2] + overstruc.lattice()[2]);


    //Copy understruc and set its lattice to heterolat, keeping its cartesian coordinates.
    //This leaves the basis intact but adds extra space where the overstruc basis can be placed.
    Structure heterostruc(understruc);

    heterostruc.set_lattice(heterolat, CART);

    //Fill up empty space in heterostruc with overstruc
    for(Index i = 0; i < overstruc.basis.size(); i++) {
      Site tsite(overstruc.basis[i]);
      tsite(CART) = tsite(CART) + understruc.lattice()[2];
      tsite.set_lattice(heterostruc.lattice(), CART);
      heterostruc.basis.push_back(tsite);

    }

    for(Index i = 0; i < heterostruc.basis.size(); i++) {
      heterostruc.basis[i].within();
    }

    heterostruc.update();

    return heterostruc;
  }
  //\John G 051112

  //John G 121212
  //***********************************************************
  /**
   * Makes reflection of structure and returns new reflected strucutre. Meant
   * for primitive structures, but can be forced on superstructures. If forced
   * on non primitive structure then the reflected structure will think it's
   * primitive. To avoid this issue reflect  primitive structure and then
   * make a reflected superlattice to fill it with.
   */
  //***********************************************************
  Structure Structure::get_reflection(bool override) const {
    if(!is_primitive()) {
      if(override) {
        std::cerr << "WARNING in Structure::get_reflection! Your structure isn't primitive but you've chosen to continue anyway." << std::endl;
        std::cerr << "Your reflected structure will have a lattice that points to itself." << std::endl;
      }

      else {
        std::cerr << "ERROR in Structure::get_reflection! Your structure isn't primitive." << std::endl;
        std::cerr << "This function is for primitive cells." << std::endl;
        exit(20);
      }
    }

    Structure reflectstruc(*this);
    Matrix3<double> zmat(0);
    zmat.at(0, 0) = 1;
    zmat.at(1, 1) = 1;
    zmat.at(2, 2) = -1;

    SymOp zmirror(zmat, lattice(), CASM::CART);

    //reflectstruc.apply_sym(zmirror);
    for(Index i = 0; i < reflectstruc.basis.size(); i++) {
      reflectstruc.basis[i].apply_sym(zmirror);
    }

    Lattice reflectlat = m_lattice.get_reflection(override);
    reflectstruc.set_lattice(reflectlat, CASM::CART);

    reflectstruc.update();

    return reflectstruc;
  }

  //***********************************************************
  /**
   * Given a minimum distance, find pair clusters within this length
   * and average out the distance between the two atoms and put
   * one atom there, eliminating the other two. Meant to be used
   * for grain boundaries.
   *
   * This function is meant for averaging atoms of the same type!
   */
  //***********************************************************
  void Structure::clump_atoms(double mindist) {

    // Warning, I don't think we want to do this here, but I'm leaving it in for now - JCT 03/07/14
    update();

    //Define Orbitree just for pairs
    SiteOrbitree siamese(lattice());
    siamese.max_num_sites = 2;
    siamese.max_length.push_back(0);
    siamese.max_length.push_back(0);
    siamese.max_length.push_back(mindist);
    siamese.min_length = 0.0;
    siamese.min_num_components = 1;
    siamese.generate_orbitree(*this);

    //Set one atom of the cluster pairs to be at the average distance and flag
    //the other one for removal later
    for(Index i = 0; i < siamese[2].size(); i++) {
      for(Index j = 0; j < siamese[2][i].size(); j++) {
        Site avgsite(siamese[2][i][j][1]);
        avgsite(CART) = (siamese[2][i][j][0](CART) + siamese[2][i][j][1](CART)) * 0.5;
        avgsite.set_lattice(lattice(), CASM::CART); //It's dumb that I need to do this
        avgsite.within();

        std::cout << "###############" << std::endl;
        std::cout << siamese[2][i][j][0].basis_ind() << "    " << siamese[2][i][j][0] << std::endl;
        std::cout << siamese[2][i][j][1].basis_ind() << "    " << siamese[2][i][j][1] << std::endl;
        std::cout << "-------" << std::endl;
        std::cout << basis[siamese[2][i][j][0].basis_ind()].basis_ind() << "    " << basis[siamese[2][i][j][0].basis_ind()] << std::endl;
        std::cout << basis[siamese[2][i][j][1].basis_ind()].basis_ind() << "    " << basis[siamese[2][i][j][1].basis_ind()] << std::endl;
        std::cout << "###############" << std::endl;


        basis[siamese[2][i][j][0].basis_ind()].set_basis_ind(-99);
        basis[siamese[2][i][j][1].basis_ind()].set_basis_ind(-99);
        basis.push_back(avgsite);
      }
    }

    std::cout << "BASIS IND:" << std::endl;
    //Remove unwanted atoms.
    int bsize = basis.size();
    for(int i = bsize - 1; i >= 0; i--) {
      std::cout << basis[i].basis_ind() << "   " << basis[i] << std::endl;
      if(basis[i].basis_ind() == Index(-99)) {
        basis.remove(i);
      }
    }

    update();

    return;
  }

  //***********************************************************
  /**
   * Loop through basis and rearrange atoms by type. Uses bubble
   * sort algorithm by comparing integer values of the strings
   * assigned to the basis occupants.
   *
   * The basis gets sorted in a sort of alphabetical way, so be
   * mindful of any POTCARs you might have if you run this.
   */
  //***********************************************************

  void Structure::sort_basis() {

    for(Index i = 0; i < basis.size(); i++) {
      for(Index j = 0; j < basis.size() - 1; j++) {

        if(basis[j].occ_name() > basis[j + 1].occ_name()) {
          basis.swap_elem(j, j + 1);
        }
      }
    }

    return;
  }



  //John G 050513

  //***********************************************************
  /**
   * Decorate your structure easily! Given a cluster, this
   * routine will copy your structure and replace sites of the
   * copy with sites from the cluster. Bedazzle!
   * The lattice of your structure and cluster must be related
   * by an integer matrix (supercell). Be mindful with the size
   * of your stamped structure, it has to be big enough to fit
   * the cluster.
   */
  //***********************************************************

  Structure Structure::stamp_with(SiteCluster stamp, bool lat_override, bool im_override) const {
    //The following check may not work properly
    if(!lat_override && !lattice().is_supercell_of((stamp.get_home()), point_group())) {
      std::cerr << "ERROR in Structure::stamp_with (are you using Structure::bedazzle?)" << std::endl;
      std::cerr << "The lattice of your cluster is not related to the lattice of the structure you want to stamp!" << std::endl;
      exit(60);
    }

    //Some sort of check should happen here to make sure your superstructure
    //is large enough to accomodate the stamp. Otherwise shit will get messed up
    //I think image_check is the way to go, but I'm not sure I'm using it correctly
    //I'm using the default value for nV (1) in image_check. This will allow the cluster
    //to touch the faces of the voronoi cell.

    if(!im_override && stamp.size() > 1 && stamp.image_check(lattice(), 1)) {    //Skip point clusters?
      std::cerr << "ERROR in Structure::stamp_with" << std::endl;
      std::cerr << "Your superstructure isn't big enough to fit your cluster!" << std::endl;
      std::cerr << "Culprit:" << std::endl;
      stamp.print(std::cerr);
      exit(62);
    }

    stamp.set_lattice(lattice(), CART);
    stamp.within();

    Structure stampedstruc = *this;
    Index sanity_count = 0;
    for(Index i = 0; i < stamp.size(); i++) {
      for(Index j = 0; j < basis.size(); j++) {
        if(Coordinate(stamp[i]) == Coordinate(basis[j])) {
          sanity_count++;
          stampedstruc.basis[j] = stamp[i];
        }
      }
    }

    if(sanity_count != stamp.size()) {  //Allow if override?
      std::cerr << "ERROR in Structure::stamp_with (are you using Structure::bedazzle?)" << std::endl;
      std::cerr << stamp.size() - sanity_count << " sites in the cluster (size " << stamp.size() << " did not map onto your structure.\
                If you got this far your lattices passed the test, but it seems your bases are unrelated. My guesses:" << std::endl;
      std::cerr << "You're trying to map a relaxed site onto an ideal one." << std::endl;
      std::cerr << "You have a vacancy in the structure you're stamping that's not in the basis." << std::endl;
      exit(61);
    }

    stampedstruc.reset();
    return stampedstruc;
  }

  //***********************************************************
  /**
   * Same as stamp_with, but takes an array of clusters and returns
   * and array of structures. Basically saves you a for loop.
   * Perhaps this should actually take an OrbitBranch?
   */
  //***********************************************************

  Array<Structure> Structure::bedazzle(Array<SiteCluster> stamps, bool lat_override, bool im_override) const {
    Array<Structure> all_decorations;
    for(Index i = 0; i < stamps.size(); i++) {
      all_decorations.push_back(stamp_with(stamps[i], lat_override, im_override));
    }

    return all_decorations;
  }

  //***********************************************************
  /**
   * Goes to a specified site of the basis and makes a flower tree
   * of pairs. It then stores the length and multiplicity of
   * the pairs in a double array, giving you a strict
   * nearest neighbor table. This version also fills up a SiteOrbitree
   * in case you want to keep it.
   * Blatantly copied from Anna's routine in old new CASM
   */
  //***********************************************************

  Array<Array<Array<double> > > Structure::get_NN_table(const double &maxr, SiteOrbitree &bouquet) {
    if(!bouquet.size()) {
      std::cerr << "WARNING in Structure::get_NN_table" << std::endl;
      std::cerr << "The provided SiteOrbitree is about to be rewritten!" << std::endl;
    }

    Array<Array<Array<double> > > NN;
    SiteOrbitree normtree(lattice());
    SiteOrbitree tbouquet(lattice());
    bouquet = tbouquet;
    normtree.min_num_components = 1;
    normtree.max_num_sites = 2;
    normtree.max_length.push_back(0.0);
    normtree.max_length.push_back(0.0);
    normtree.max_length.push_back(maxr);

    normtree.generate_orbitree(*this);
    normtree.print_full_clust(std::cout);
    generate_basis_bouquet(normtree, bouquet, 2);

    Array<Array<double> > oneNN;
    oneNN.resize(2);
    for(Index i = 0; i < bouquet.size(); i++) {
      NN.push_back(oneNN);
      for(Index j = 0; j < bouquet[i].size(); j++) {
        NN[i][0].push_back(bouquet[i][j].size());
        NN[i][1].push_back(bouquet[i][j].max_length());
      }
    }

    return NN;
  }

  //***********************************************************
  /**
   * Goes to a specified site of the basis and makes a flower tree
   * of pairs. It then stores the length and multiplicity of
   * the pairs in a double array, giving you a strict
   * nearest neighbor table. The bouquet used for this
   * falls into the void.
   */
  //***********************************************************

  Array<Array<Array<double> > > Structure::get_NN_table(const double &maxr) {
    SiteOrbitree bouquet(lattice());
    return get_NN_table(maxr, bouquet);
  }

  //***********************************************************
  /**
   * Given a symmetry group, the basis of the structure will have
   * each operation applied to it. The resulting set of basis
   * from performing these operations will be averaged out,
   * yielding a new average basis that will replace the current one.
   */
  //***********************************************************

  void Structure::symmetrize(const SymGroup &relaxed_factors) {
    //First make a copy of your current basis
    //This copy will eventually become the new average basis.
    reset();
    Array<Site> avg_basis = basis;

    //Loop through given symmetry group an fill a temporary "operated basis"
    Array<Site> operbasis;
    for(Index rf = 0; rf < relaxed_factors.size(); rf++) {
      operbasis.clear();
      for(Index b = 0; b < basis.size(); b++) {
        operbasis.push_back(relaxed_factors[rf]*basis[b]);
        operbasis.back().print(std::cout);
        std::cout << std::endl;
      }
      //Now that you have a transformed basis, find the closest mapping of atoms
      //Then average the distance and add it to the average basis
      for(Index b = 0; b < basis.size(); b++) {
        double smallest = 1000000;
        Coordinate bshift(lattice()), tshift(lattice());
        for(Index ob = 0; ob < operbasis.size(); ob++) {
          double dist = operbasis[ob].min_dist(basis[b], tshift);
          if(dist < smallest) {
            bshift = tshift;
            smallest = dist;
          }
        }
        bshift(CART) *= (1.0 / relaxed_factors.size());
        avg_basis[b] += bshift;
      }

    }
    //generate_factor_group();
    update();
    return;
  }

  //***********************************************************
  /**
   * Same as the other symmetrize routine, except this one assumes
   * that the symmetry group you mean to use is the factor group
   * of your structure within a certain tolerance.
   *
   * Notice that the tolerance is also used on your point group!!
   */
  //***********************************************************

  void Structure::symmetrize(const double &tolerance) {
    generate_factor_group(tolerance);
    symmetrize(factor_group());
    return;
  }

  //\John G 050513

  //***********************************************************
  /**
   * Using char as a way to specify which occupation basis you're interested
   * in. This routine will call the appropriate methods in every
   * site to fill the basis sets with functions
   */
  //***********************************************************

  void Structure::fill_occupant_bases(const char &basis_type) {
    for(Index i = 0; i < basis.size(); i++) {
      basis[i].fill_occupant_basis(basis_type);
    }
    return;
  }

  //***********************************************************
  /**
   *  Call this on a structure to get new_surface_struc: the structure with a
   *  layer of vacuum added parallel to the ab plane.
   *  vacuum_thickness: thickness of vacuum layer (Angstroms)
   *  shift:  shift vector from layer to layer, assumes FRAC unless specified.
   *  The shift vector should only have values relative to a and b vectors (eg x, y, 0).
   *  Default shift is zero.
   */
  //***********************************************************

  void Structure::add_vacuum_shift(Structure &new_surface_struc, double vacuum_thickness, Vector3<double> shift, COORD_TYPE mode) const {

    Coordinate cshift(shift, lattice(), mode);    //John G 121030
    if(!almost_zero(cshift(FRAC)[2])) {
      std::cerr << cshift(FRAC) << std::endl;
      std::cerr << "WARNING: You're shifting in the c direction! This will mess with your vacuum and/or structure!!" << std::endl;
      std::cerr << "See Structure::add_vacuum_shift" << std::endl;
    }

    Vector3<double> vacuum_vec;                 //unit vector perpendicular to ab plane
    vacuum_vec = lattice()[0].cross(lattice()[1]);
    vacuum_vec.normalize();
    Lattice new_lattice(lattice()[0],
                        lattice()[1],
                        lattice()[2] + vacuum_thickness * vacuum_vec + cshift(CART)); //Add vacuum and shift to c vector

    new_surface_struc = *this;
    new_surface_struc.set_lattice(new_lattice, CART);
    return;
  }

  //***********************************************************
  void Structure::add_vacuum_shift(Structure &new_surface_struc, double vacuum_thickness, Coordinate shift) const {
    if(shift.get_home() != &lattice()) {
      std::cerr << "WARNING: The lattice from your shift coordinate does not match the lattice of your structure!" << std::endl;
      std::cerr << "See Structure::add_vacuum_shift" << std::endl << std::endl;
    }

    add_vacuum_shift(new_surface_struc, vacuum_thickness, shift(CART), CART);
    return;
  }

  //***********************************************************
  void Structure::add_vacuum(Structure &new_surface_struc, double vacuum_thickness) const {
    Vector3<double> shift(0, 0, 0);

    add_vacuum_shift(new_surface_struc, vacuum_thickness, shift, FRAC);

    return;
  }

  // Added by Donghee

  //***********************************************************
  /**
   * Creates N of image POSCAR files in seperate directories by
   * interpolating linearly between structures.
   * For exact interpolation, choose " LOCAL " or "1" in mode
   * For nearest-image interpolation, chosse "PERIODIC" or "0"
   *
   * Stored in Array<Structure> images;
   * images[0] = start structure, images[Nofimage+1] = end structure
   */
  //***********************************************************

  void Structure::intpol(Structure end_struc, int Nofimag, PERIODICITY_TYPE mode, Array<Structure> &images) {

    for(int m = 0; m < Nofimag + 1; m++) {

      Structure tstruc(end_struc);

      // Change title
      std::string header = "POSCAR 0";
      std::stringstream convert; // stringstream used for the conversion;
      convert << m;
      tstruc.title = header + convert.str(); // title of submiges is POSCAR0X


      for(Index i = 0 ; i < basis.size(); i++) {
        Coordinate temp(lattice());

        temp(FRAC) = (basis[i] - end_struc.basis[i])(FRAC) * m / (Nofimag + 1);

        tstruc.basis[i] = basis[i] + temp;

      }

      images.push_back(tstruc);

    }
    images.push_back(end_struc);

    return ;
  }

  //*****************************************************************************
  /**
     Creates num_images structures that are linearly interpolated between (*this)
     and the end_struc
     CAUTION: Use with caution currently. It DOES NOT check to find the atom closest to
     itself in the end_struc, so if the basis sites are reorganized, structures
     are bound to get messed up. Also ensure that the end_struc lattice is simply
     a strained version of the current lattice, and that there are no rotations
     in it. Or, for that matter that it is the same lattice that has been rela-
     xed in some way
     Checks and fixes coming soon
  */
  //*****************************************************************************
  /*
  void Structure::linear_interpolate(Structure end_struc, int num_images, Array<Structure> &images) {
    std::cerr << "WARNING: This function assumes you are passing it structures in a certain way. I hope you know what you are doing.\n";
    Array<Lattice> interp_lat;
    lattice().linear_interpolate(end_struc.lattice(), num_images, interp_lat);
    Array<Coordinate> increment_coord;
    for(Index i = 0; i < basis.size(); i++) {
      Vector3<double> ttrans;
      for(int j = 0; j < 3; j++) {
        ttrans[j] = (end_struc.basis[i](CART)[j] - basis[i](CART)[j]) / double(num_images + 1);
      }
      Coordinate tCoord(ttrans, lattice(), CART);
      increment_coord.push_back(tCoord);
    }
    for(int i = 0; i <= (num_images + 1); i++) {
      Structure tstruc(*this);
      for(Index j = 0; j < basis.size(); j++) {
        tstruc.basis[j].update(CART);
        tstruc.basis[j](CART) += increment_coord[j](CART) * i;
        tstruc.basis[j].update(FRAC);
      }
      tstruc.set_lattice(interp_lat[i], CART);
      images.push_back(tstruc);
    }
    return;
  }
  */
  //***********************************************************
  /**
   * Create numbered subdirectories and copies the appropriate
   * POSCAR files into those.
   * you can choose where you want to create subdirectories and name
   */
  //***********************************************************
  void Structure::print_hop_images(Array<Structure> images, std::string location) {

    std::ofstream out;
    if(mkdir(location.c_str(), 0777) == -1) {
      std::cerr << " This Directory is already existed, it will be over-written in previous files.  \n";
    };



    for(Index i = 0; i < images.size() ; i ++) {
      std::stringstream convert;
      std::string imag_num, POSCAR, name;

      convert << i;
      if(i > 10) {
        name = location + "/POSCAR" + convert.str();
      }
      else {
        name = location + "/POSCAR0" + convert.str();
      }

      out.open(name.c_str());
      images[i].print(out);
      out.close();

      if(i > 10) {
        imag_num = location + "/" + convert.str(); //it the name of subdirectories 10, 11
        POSCAR = imag_num + "/POSCAR";
      }
      else {
        imag_num = location + "/0" + convert.str(); // 01 02
        POSCAR = imag_num + "/POSCAR";
      }

      if(mkdir(imag_num.c_str(), 0777) == -1) {
        ;
      }

      out.open(POSCAR.c_str());
      if(out.is_open()) {
        images[i].print(out);
      }
      else {
        std::cerr << "ERROR creating POSCAR " << i << "\n";
      }
      out.close();


    }

    return;
  }


  //***********************************************************

  //This function gets the permutation representation of the
  // factor group operations of the structure. It first applies
  // the factor group operation to the structure, and then tries
  // to map the new position of the basis atom to the various positions
  // before symmetry was applied. It only checks the positions after
  // it brings the basis within the crystal.

  // ROUTINE STILL NEEDS TO BE TESTED!

  //***********************************************************

  // non-const version - calculates factor group if it doesn't already exist
  Index Structure::generate_permutation_representation(bool verbose) const {
    perm_rep_ID = BasicStructure<Site>::generate_permutation_representation(factor_group(), verbose);

    return perm_rep_ID;
  }

  //***********************************************************

  //This function gets the basis_permutation representation of the
  // factor group operations of the structure. It first applies
  // the factor group operation to the structure, and then tries
  // to map the new position of the basis atom to the various positions
  // before symmetry was applied. It only checks the positions after
  // it brings the basis within the crystal.

  // ROUTINE STILL NEEDS TO BE TESTED!

  // non-const version - calculates factor group if it doesn't already exist
  Index Structure::generate_basis_permutation_representation(bool verbose) const {
    basis_perm_rep_ID = BasicStructure<Site>::generate_basis_permutation_representation(factor_group(), verbose);

    return basis_perm_rep_ID;
  }

  //***********************************************************

  //Sets the occupants in the basis sites to those specified by occ_index
  void Structure::set_occs(Array <int> occ_index) {
    if(occ_index.size() != basis.size()) {
      std::cerr << "The size of the occ index and basis index do not match!\nEXITING\n";
      exit(1);
    }
    for(Index i = 0; i < basis.size(); i++) {
      basis[i].set_occ_value(occ_index[i]);
    }
  }

  //***********************************************************
  Structure &Structure::operator+=(const Coordinate &shift) {

    for(Index i = 0; i < basis.size(); i++) {
      basis[i] += shift;
    }

    factor_group_internal += shift;
    return (*this);
  }


  //***********************************************************
  Structure &Structure::operator-=(const Coordinate &shift) {

    for(Index i = 0; i < basis.size(); i++) {
      basis[i] -= shift;
    }
    factor_group_internal -= shift;
    return (*this);
  }

  //****************************************************

  jsonParser &Structure::to_json(jsonParser &json) const {

    // class Structure : public BasicStructure<Site>
    BasicStructure<Site>::to_json(json);

    // mutable MasterSymGroup factor_group_internal;
    json["factor_group"] = factor_group_internal;

    // mutable int perm_rep_id;
    json["perm_rep_ID"] = perm_rep_ID;

    // bool SD_flag;
    json["SD_flag"] = SD_flag;

    return json;
  }

  //****************************************************

  // Assumes constructor CoordType::CoordType(Lattice) exists
  void Structure::from_json(const jsonParser &json) {
    try {

      // class Structure : public BasicStructure<Site>
      BasicStructure<Site> &basic = *this;
      basic.from_json(json);

      // mutable MasterSymGroup factor_group_internal;
      factor_group_internal.clear();
      Coordinate coord(lattice());
      factor_group_internal.push_back(SymOp(coord));
      factor_group_internal.from_json(json["factor_group"]);

      // mutable int perm_rep_ID;
      CASM::from_json(perm_rep_ID, json["perm_rep_ID"]);

      // bool SD_flag;
      CASM::from_json(SD_flag, json["SD_flag"]);

    }
    catch(...) {
      /// re-throw exceptions
      throw;
    }
    update();
  }

  //****************************************************

  jsonParser &to_json(const Structure &structure, jsonParser &json) {
    return structure.to_json(json);
  }

  void from_json(Structure &structure, const jsonParser &json) {
    structure.from_json(json);
  }

  //***********************************************************
  /*
  Structure operator*(const SymOp &LHS, const Structure &RHS) { //AAB

    return Structure(RHS).apply_sym(LHS);
  }
  */
  //***********************************************************
  Structure operator*(const Lattice &LHS, const Structure &RHS) {
    Structure tsuper(LHS);
    tsuper.fill_supercell(RHS);
    return tsuper;
  }






  /// Helper Functions


  /// Returns 'converter' which converts Site::site_occupant indices to 'mol_list' indices:
  ///   mol_list_index = converter[basis_site][site_occupant_index]
  Array< Array<int> > get_index_converter(const Structure &struc, Array<Molecule> mol_list) {

    Array< Array<int> > converter(struc.basis.size());

    for(Index i = 0; i < struc.basis.size(); i++) {
      converter[i].resize(struc.basis[i].site_occupant().size());

      for(Index j = 0; j < struc.basis[i].site_occupant().size(); j++) {
        converter[i][j] = mol_list.find(struc.basis[i].site_occupant()[j]);
      }
    }

    return converter;

  }

  /// Returns 'converter' which converts Site::site_occupant indices to 'mol_name_list' indices:
  ///   mol_name_list_index = converter[basis_site][site_occupant_index]
  Array< Array<int> > get_index_converter(const Structure &struc, Array<std::string> mol_name_list) {

    Array< Array<int> > converter(struc.basis.size());

    for(Index i = 0; i < struc.basis.size(); i++) {
      converter[i].resize(struc.basis[i].site_occupant().size());

      for(Index j = 0; j < struc.basis[i].site_occupant().size(); j++) {
        converter[i][j] = mol_name_list.find(struc.basis[i].site_occupant()[j].name);
      }
    }

    return converter;

  }

};


