#ifndef CASM_CompositionConverter_HH
#define CASM_CompositionConverter_HH

#include <vector>
#include <map>
#include "casm/external/Eigen/Dense"

#include "casm/casm_io/jsonParser.hh"
#include "casm/clex/ParamComposition.hh"

namespace CASM {

  class Structure;

  class CompositionConverter {

  public:

    typedef unsigned int size_type;

    /// \brief Default constructor
    CompositionConverter() {}

    /// \brief Construct a CompositionConverter
    template<typename ComponentIterator, typename... EndMembers>
    CompositionConverter(ComponentIterator begin, ComponentIterator end, Eigen::VectorXd _origin, EndMembers... _end_members);

    /// \brief Construct a CompositionConverter
    template<typename ComponentIterator>
    CompositionConverter(ComponentIterator begin, ComponentIterator end, Eigen::VectorXd _origin, Eigen::MatrixXd _end_members);


    /// \brief The dimensionality of the composition space
    size_type independent_compositions() const;

    /// \brief The order of components in mol composition vectors
    std::vector<std::string> components() const;

    /// \brief The mol composition of the parameteric composition axes origin
    Eigen::VectorXd origin() const;

    /// \brief The mol composition of the parameteric composition axes end members
    Eigen::VectorXd end_member(size_type i) const;

    /// \brief Convert number of mol per prim, 'n' to parametric composition 'x'
    Eigen::VectorXd param_composition(const Eigen::VectorXd &n) const;

    /// \brief Convert parametric composition, 'x', to number of mol per prim, 'n'
    Eigen::VectorXd mol_composition(const Eigen::VectorXd &n) const;

    /// \brief Convert dG/dn to dG/dx
    Eigen::VectorXd param_mu(const Eigen::VectorXd atomic_mu) const;

    /// \brief Convert dG/dx to dG/dn
    Eigen::VectorXd atomic_mu(const Eigen::VectorXd param_mu) const;


    /// \brief Return formula for x->n
    std::string mol_formula() const;

    /// \brief Return formula for n->x
    std::string param_formula() const;

    /// \brief Return formula for origin
    std::string origin_formula() const;

    /// \brief Return formula for end member
    std::string end_member_formula(size_type i) const;


  private:

    /// \brief Helps make variadic template constructor possible
    void _add_end_member(Eigen::VectorXd _end_member);

    /// \brief Helps make variadic template constructor possible
    template<typename... EndMembers>
    void _add_end_member(Eigen::VectorXd _end_member, EndMembers... _others);

    /// \brief Check that origin and end member vectors have same size as the number of components
    void _check_size(const Eigen::VectorXd &vec) const;

    /// \brief Calculate conversion matrices m_to_n and m_to_x
    void _calc_conversion_matrices();

    /// \brief Return formula for 'n'
    std::string _n_formula(const Eigen::VectorXd &vec) const;


    /// \brief List of all allowed components names in the prim, position in vector is reference
    ///  for origin and end_members
    std::vector<std::string> m_components;

    /// \brief Vector, size == m_components.size(), specifying the num_mols_per_prim of each
    ///  component at the origin in parametric composition space
    Eigen::VectorXd m_origin;

    /// \brief Column vector matrix, rows == m_components.size(), cols == rank of parametric composition space
    /// - Specifies the number mol per prim of end member in parametric composition space
    Eigen::MatrixXd m_end_members;

    /// \brief Conversion matrix: n = origin + m_to_n*x
    /// - where x is parametric composition, and n is number of mol per prim
    Eigen::MatrixXd m_to_n;

    /// \brief Conversion matrix: x = m_to_x*(n - origin)
    /// - where x is parametric composition, and n is number of mol per prim
    Eigen::MatrixXd m_to_x;

  };

  /// \brief Generate CompositionConverter specifying standard composition axes for a prim Structure
  template<typename OutputIterator>
  OutputIterator standard_composition_axes(const Structure &prim, OutputIterator result);

  /// \brief Pretty-print map of name/CompositionConverter pairs
  void display_composition_axes(std::ostream &stream, const std::map<std::string, CompositionConverter> &map);

  /// \brief Serialize CompositionConverter to JSON
  jsonParser &to_json(const CompositionConverter &f, jsonParser &json);

  /// \brief Deserialize CompositionConverter from JSON
  void from_json(CompositionConverter &f, const jsonParser &json);


  // ------ Definitions ---------------------------------------------

  /// \brief Construct a CompositionConverter
  ///
  /// \param begin,end Range of occupant name (std::string) indicating the Molecule type
  /// \param _origin Origin for parametric composition space
  /// \param _end_members 1 or more end members for parameteric composition space axes
  ///
  /// - origin and end members should be Eigen::VectorXd giving number of atoms per type per prim
  /// - length of origin and end members should match size of prim.get_struc_molecule()
  ///
  template<typename ComponentIterator, typename... EndMembers>
  CompositionConverter::CompositionConverter(ComponentIterator begin,
                                             ComponentIterator end,
                                             Eigen::VectorXd _origin,
                                             EndMembers... _end_members) :
    m_components(begin, end),
    m_origin(_origin) {

    _add_end_member(_end_members...);

    _check_size(_origin);

    _calc_conversion_matrices();

  }

  /// \brief Construct a CompositionConverter
  ///
  /// \param _prim A primitive Structure
  /// \param _origin Origin for parametric composition space
  /// \param _end_members Column vector matrix of end members for parameteric composition space axes
  ///
  /// - origin and columns in end members should be Eigen::VectorXd giving number of atoms per prim
  /// - length of origin and end members should match size of prim.get_struc_molecule()
  ///
  template<typename ComponentIterator>
  CompositionConverter::CompositionConverter(ComponentIterator begin,
                                             ComponentIterator end,
                                             Eigen::VectorXd _origin,
                                             Eigen::MatrixXd _end_members) :
    m_components(begin, end),
    m_origin(_origin),
    m_end_members(_end_members) {

    _check_size(_origin);

    _check_size(_end_members.col(0));

    _calc_conversion_matrices();

  }

  /// \brief Helps make variadic template constructor possible
  template<typename... EndMembers>
  void CompositionConverter::_add_end_member(Eigen::VectorXd _end_member, EndMembers... _others) {
    _add_end_member(_end_member);
    _add_end_member(_others...);
  }

  /// \brief Generate CompositionConverter specifying standard composition axes for a prim Structure
  template<typename OutputIterator>
  OutputIterator standard_composition_axes(const Structure &prim, OutputIterator result) {
    ParamComposition param_comp(prim);
    param_comp.generate_components();
    param_comp.generate_sublattice_map();
    param_comp.generate_prim_end_members();
    param_comp.generate_composition_space();

    std::vector<std::string> components;
    for(int i = 0; i < param_comp.get_components().size(); i++) {
      components.push_back(param_comp.get_components()[i]);
    }

    for(int i = 0; i < param_comp.get_allowed_list().size(); i++) {
      const ParamComposition &curr = param_comp.get_allowed_list()[i];
      Eigen::MatrixXd end_members(curr.get_origin().size(), curr.get_spanning_end_members().size());
      for(int j = 0; j < curr.get_spanning_end_members().size(); j++) {
        end_members.col(j) = curr.get_spanning_end_members()[j];
      }

      *result++ = CompositionConverter(components.begin(), components.end(), curr.get_origin(), end_members);
    }

    return result;
  }
}

#endif

