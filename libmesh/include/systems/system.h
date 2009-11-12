// $Id$

// The libMesh Finite Element Library.
// Copyright (C) 2002-2008 Benjamin S. Kirk, John W. Peterson, Roy H. Stogner
  
// This library is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 2.1 of the License, or (at your option) any later version.
  
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Lesser General Public License for more details.
  
// You should have received a copy of the GNU Lesser General Public
// License along with this library; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA



#ifndef __system_h__
#define __system_h__

// C++ includes
#include <vector>
#include <set>

// Local Includes
#include "auto_ptr.h"
#include "elem_range.h"
#include "enum_xdr_mode.h"
#include "fe_type.h"
#include "libmesh_common.h"
#include "qoi_set.h"
#include "reference_counted_object.h"
#include "system_norm.h"

// Forward Declarations
class System;
class EquationSystems;
class MeshBase;
class Xdr;
class DofMap;
class Parameters;
class ParameterVector;
class Point;
class SensitivityData;
template <typename T> class NumericVector;
template <typename T> class VectorValue;
typedef VectorValue<Number> NumberVectorValue;
typedef NumberVectorValue Gradient;

/**
 * This is the base class for classes which contain 
 * information related to any physical process that might be simulated.  
 * Such information may range from the actual solution values to 
 * algorithmic flags that may be used to control the numerical methods
 * employed.  In general, use an \p EqnSystems<T_sys> object to handle
 * one or more of the children of this class.
 * Note that templating \p EqnSystems relaxes the use of virtual members.
 *
 * @author Benjamin S. Kirk, 2003-2004.
 */

// ------------------------------------------------------------
// System class definition
class System : public ReferenceCountedObject<System>
{
protected:

  /**
   * Constructor.  Optionally initializes required
   * data structures.  Protected so that this base class
   * cannot be explicitly instantiated.
   */
  System (EquationSystems& es,
	  const std::string& name,
	  const unsigned int number);
  
public:

  
  /**
   * Destructor.
   */
  virtual ~System ();

  /**
   * The type of system.
   */
  typedef System sys_type;

  /**
   * @returns a clever pointer to the system.
   */
  sys_type & system () { return *this; }
  
  /**
   * Clear all the data structures associated with
   * the system.
   */
  virtual void clear ();
  
  /** 
   * Initializes degrees of freedom on the current mesh.
   * Sets the 
   */
  void init ();
  
  /**
   * Reinitializes degrees of freedom and other 
   * required data on the current mesh.  Note that the matrix
   * is not initialized at this time since it may not be required
   * for all applications. @e Should be overloaded in derived classes.
   */
  virtual void reinit ();
  
  /**
   * Update the local values to reflect the solution
   * on neighboring processors.
   */
  virtual void update ();
  
  /**
   * Prepares \p matrix and \p _dof_map for matrix assembly.
   * Does not actually assemble anything.  For matrix assembly,
   * use the \p assemble() in derived classes.
   * @e Should be overloaded in derived classes.
   */
  virtual void assemble ();

  /**
   * Calls user qoi function.
   * @e Can be overloaded in derived classes.
   */
  virtual void assemble_qoi
    (const QoISet &qoi_indices = QoISet());
 
  /**
   * Calls user qoi derivative function.
   * @e Can be overloaded in derived classes.
   */
  virtual void assemble_qoi_derivative 
    (const QoISet &qoi_indices = QoISet());
 
  /**
   * Calls residual parameter derivative function.
   *
   * Library subclasses use finite differences by default.
   *
   * This should assemble the sensitivity rhs vectors to hold
   * -(partial R / partial p_i), making them ready to solve
   * the forward sensitivity equation.
   *
   * This method is only implemented in some derived classes.
   */
  virtual void assemble_residual_derivatives (const ParameterVector& parameters);
 
  /**
   * Solves the system.  Must be overloaded in derived systems.
   */
  virtual void solve () = 0;
  
  /**
   * Solves the sensitivity system, for the provided parameters.
   * Must be overloaded in derived systems.
   *
   * Returns a pair with the total number of linear iterations
   * performed and the (sum of the) final residual norms
   *
   * This method is only implemented in some derived classes.
   */
  virtual std::pair<unsigned int, Real>
    sensitivity_solve (const ParameterVector& parameters);
  
  /**
   * Assembles & solves the linear system(s) (dR/du)*u_w = sum(w_p*-dR/dp), for
   * those parameters p contained within \p parameters weighted by the
   * values w_p found within \p weights.
   *
   * Returns a pair with the total number of linear iterations
   * performed and the (sum of the) final residual norms
   *
   * This method is only implemented in some derived classes.
   */
  virtual std::pair<unsigned int, Real>
    weighted_sensitivity_solve (const ParameterVector& parameters,
                                const ParameterVector& weights);
 
  /**
   * Solves the adjoint system, for the specified qoi indices, or for
   * every qoi if \p qoi_indices is NULL.  Must be overloaded in
   * derived systems.
   *
   * Returns a pair with the total number of linear iterations
   * performed and the (sum of the) final residual norms
   *
   * This method is only implemented in some derived classes.
   */
  virtual std::pair<unsigned int, Real>
    adjoint_solve (const QoISet& qoi_indices = QoISet());
  
  /**
   * Assembles & solves the linear system(s) 
   * (dR/du)^T*z_w = sum(w_p*(d^2q/dudp - d^2R/dudp*z)), for those
   * parameters p contained within \p parameters, weighted by the
   * values w_p found within \p weights.
   *
   * Assumes that adjoint_solve has already calculated z for each qoi
   * in \p qoi_indices.
   *
   * Returns a pair with the total number of linear iterations
   * performed and the (sum of the) final residual norms
   *
   * This method is only implemented in some derived classes.
   */
  virtual std::pair<unsigned int, Real>
    weighted_sensitivity_adjoint_solve (const ParameterVector& parameters,
                                        const ParameterVector& weights,
                                        const QoISet& qoi_indices = QoISet());
 
  /**
   * Solves for the derivative of each of the system's quantities of
   * interest q in \p qoi[qoi_indices] with respect to each parameter
   * in \p parameters, placing the result for qoi \p i and parameter
   * \p j into \p sensitivities[i][j].  
   *
   * Note that parameters is a const vector, not a vector-of-const;
   * parameter values in this vector need to be mutable for finite
   * differencing to work.
   *
   * Automatically chooses the forward method for problems with more
   * quantities of interest than parameters, or the adjoint method
   * otherwise.
   *
   * This method is only usable in derived classes which overload
   * an implementation.
   */
  virtual void qoi_parameter_sensitivity (const QoISet& qoi_indices,
                                          const ParameterVector& parameters,
                                          SensitivityData& sensitivities);
  
  /**
   * Solves for parameter sensitivities using the adjoint method.
   *
   * This method is only implemented in some derived classes.
   */
  virtual void adjoint_qoi_parameter_sensitivity (const QoISet& qoi_indices,
                                                  const ParameterVector& parameters,
                                                  SensitivityData& sensitivities);
  
  /**
   * Solves for parameter sensitivities using the forward method.
   *
   * This method is only implemented in some derived classes.
   */
  virtual void forward_qoi_parameter_sensitivity (const QoISet& qoi_indices,
                                                  const ParameterVector& parameters,
                                                  SensitivityData& sensitivities);
  
  /**
   * For each of the system's quantities of interest q in 
   * \p qoi[qoi_indices], and for a vector of parameters p, the
   * parameter sensitivity Hessian H_ij is defined as 
   * H_ij = (d^2 q)/(d p_i d p_j)
   * This Hessian is the output of this method, where for each q_i,
   * H_jk is stored in \p hessian.second_derivative(i,j,k).  
   *
   * This method is only implemented in some derived classes.
   */
  virtual void qoi_parameter_hessian(const QoISet& qoi_indices,
                                     const ParameterVector& parameters,
                                     SensitivityData& hessian);

  /**
   * For each of the system's quantities of interest q in 
   * \p qoi[qoi_indices], and for a vector of parameters p, the
   * parameter sensitivity Hessian H_ij is defined as 
   * H_ij = (d^2 q)/(d p_i d p_j)
   * The Hessian-vector product, for a vector v_k in parameter space, is
   * S_j = H_jk v_k
   * This product is the output of this method, where for each q_i,
   * S_j is stored in \p sensitivities[i][j].  
   *
   * This method is only implemented in some derived classes.
   */
  virtual void qoi_parameter_hessian_vector_product(const QoISet& qoi_indices,
                                                    const ParameterVector& parameters,
                                                    const ParameterVector& vector,
                                                    SensitivityData& product);
  
  /**
   * @returns \p true when the other system contains
   * identical data, up to the given threshold.  Outputs
   * some diagnostic info when \p verbose is set.
   */
  virtual bool compare (const System& other_system, 
			const Real threshold,
			const bool verbose) const;

  /**
   * @returns the system name.
   */
  const std::string & name () const;

  /**
   * @returns the type of system, helpful in identifying
   * which system type to use when reading equation system
   * data from file.  Should be overloaded in derived classes.
   */
  virtual std::string system_type () const { return "BasicSystem"; }

  /**
   * Projects the continuous functions onto the current solution. 
   */
  void project_solution (Number fptr(const Point& p,
				     const Parameters& parameters,
                                     const std::string& sys_name,
				     const std::string& unknown_name),
                         Gradient gptr(const Point& p,
				       const Parameters& parameters,
                                       const std::string& sys_name,
				       const std::string& unknown_name),
			 Parameters& parameters) const;

  /**
   * Projects the continuous functions onto the current mesh.
   */
  void project_vector (Number fptr(const Point& p,
				   const Parameters& parameters,
                                   const std::string& sys_name,
				   const std::string& unknown_name),
                       Gradient gptr(const Point& p,
				     const Parameters& parameters,
                                     const std::string& sys_name,
				     const std::string& unknown_name),
		       Parameters& parameters,
		       NumericVector<Number>& new_vector) const;
  
  /**
   * @returns the system number.   
   */
  unsigned int number () const;
  
  /**
   * Fill the input vector \p global_soln so that it contains
   * the global solution on all processors.
   * Requires communication with all other processors.
   */
  void update_global_solution (std::vector<Number>& global_soln) const;

  /**
   * Fill the input vector \p global_soln so that it contains
   * the global solution on processor \p dest_proc.
   * Requires communication with all other processors.
   */
  void update_global_solution (std::vector<Number>& global_soln,
			       const unsigned int dest_proc) const;

  /**
   * @returns a constant reference to this systems's \p _mesh.
   */
  const MeshBase & get_mesh() const;
  
  /**
   * @returns a reference to this systems's \p _mesh.
   */
  MeshBase & get_mesh();
  
  /**
   * @returns a constant reference to this system's \p _dof_map.
   */
  const DofMap & get_dof_map() const;
  
  /**
   * @returns a writeable reference to this system's \p _dof_map.
   */
  DofMap & get_dof_map();

  /**
   * @returns a constant reference to this system's parent EquationSystems object.
   */
  const EquationSystems & get_equation_systems() const { return _equation_systems; }

  /**
   * @returns a reference to this system's parent EquationSystems object.
   */
  EquationSystems & get_equation_systems() { return _equation_systems; }

  /**
   * @returns \p true if the system is active, \p false otherwise.
   * An active system will be solved.
   */
  bool active () const;

  /**
   * Activates the system.  Only active systems are solved.
   */
  void activate ();

  /**
   * Deactivates the system.  Only active systems are solved.
   */
  void deactivate ();

  /**
   * Vector iterator typedefs.
   */
  typedef std::map<std::string, NumericVector<Number>* >::iterator       vectors_iterator;
  typedef std::map<std::string, NumericVector<Number>* >::const_iterator const_vectors_iterator;

  /**
   * Beginning of vectors container
   */
  vectors_iterator vectors_begin ();

  /**
   * Beginning of vectors container
   */
  const_vectors_iterator vectors_begin () const;

  /**
   * End of vectors container
   */
  vectors_iterator vectors_end ();

  /**
   * End of vectors container
   */
  const_vectors_iterator vectors_end () const;

  /**
   * Adds the additional vector \p vec_name to this system.  Only
   * allowed @e prior to \p init().  All the additional vectors
   * are similarly distributed, like the \p solution,
   * and inititialized to zero.
   * 
   * By default vectors added by add_vector are projected to changed grids by
   * reinit().  To zero them instead (more efficient), pass "false" as the
   * second argument
   */
  NumericVector<Number> & add_vector (const std::string& vec_name,
				      const bool projections=true);

  /**
   * Tells the System whether or not to project the solution vector onto new
   * grids when the system is reinitialized.  The solution will be projected
   * unless project_solution_on_reinit() = false is called.
   */
  bool& project_solution_on_reinit (void)
    { return _solution_projection; }
  
  /**
   * @returns \p true if this \p System has a vector associated with the
   * given name, \p false otherwise.
   */
  bool have_vector (const std::string& vec_name) const;
  
  /**
   * @returns a const pointer to the vector if this \p System has a
   * vector associated with the given name, \p NULL otherwise.
   */
  const NumericVector<Number> * request_vector (const std::string& vec_name) const;
  
  /**
   * @returns a pointer to the vector if this \p System has a
   * vector associated with the given name, \p NULL otherwise.
   */
  NumericVector<Number> * request_vector (const std::string& vec_name);
  
  /**
   * @returns a const pointer to this system's @e additional vector
   * number \p vec_num (where the vectors are counted starting with
   * 0), or returns \p NULL if the system has no such vector.
   */
  const NumericVector<Number> * request_vector (const unsigned int vec_num) const;

  /**
   * @returns a writeable pointer to this system's @e additional
   * vector number \p vec_num (where the vectors are counted starting
   * with 0), or returns \p NULL if the system has no such vector.
   */
  NumericVector<Number> * request_vector (const unsigned int vec_num);

  /**
   * @returns a const reference to this system's @e additional vector
   * named \p vec_name.  Access is only granted when the vector is already
   * properly initialized.
   */
  const NumericVector<Number> & get_vector (const std::string& vec_name) const;

  /**
   * @returns a writeable reference to this system's @e additional vector
   * named \p vec_name.  Access is only granted when the vector is already
   * properly initialized.
   */
  NumericVector<Number> & get_vector (const std::string& vec_name);

  /**
   * @returns a const reference to this system's @e additional vector
   * number \p vec_num (where the vectors are counted starting with
   * 0).
   */
  const NumericVector<Number> & get_vector (const unsigned int vec_num) const;

  /**
   * @returns a writeable reference to this system's @e additional
   * vector number \p vec_num (where the vectors are counted starting
   * with 0).
   */
  NumericVector<Number> & get_vector (const unsigned int vec_num);

  /**
   * @returns the name of this system's @e additional vector number \p
   * vec_num (where the vectors are counted starting with 0).
   */
  const std::string & vector_name (const unsigned int vec_num);

  /**
   * @returns a reference to one of the system's adjoint solution
   * vectors, by default the one corresponding to the first qoi.
   * Creates the vector if it doesn't already exist.
   */
  NumericVector<Number> & add_adjoint_solution(unsigned int i=0);

  /**
   * @returns a reference to one of the system's adjoint solution
   * vectors, by default the one corresponding to the first qoi.
   */
  NumericVector<Number> & get_adjoint_solution(unsigned int i=0);

  /**
   * @returns a reference to one of the system's adjoint solution
   * vectors, by default the one corresponding to the first qoi.
   */
  const NumericVector<Number> & get_adjoint_solution(unsigned int i=0) const;

  /**
   * @returns a reference to one of the system's solution sensitivity
   * vectors, by default the one corresponding to the first parameter.
   * Creates the vector if it doesn't already exist.
   */
  NumericVector<Number> & add_sensitivity_solution(unsigned int i=0);

  /**
   * @returns a reference to one of the system's solution sensitivity
   * vectors, by default the one corresponding to the first parameter.
   */
  NumericVector<Number> & get_sensitivity_solution(unsigned int i=0);

  /**
   * @returns a reference to one of the system's solution sensitivity
   * vectors, by default the one corresponding to the first parameter.
   */
  const NumericVector<Number> & get_sensitivity_solution(unsigned int i=0) const;

  /**
   * @returns a reference to one of the system's weighted sensitivity
   * adjoint solution vectors, by default the one corresponding to the
   * first qoi.
   * Creates the vector if it doesn't already exist.
   */
  NumericVector<Number> & add_weighted_sensitivity_adjoint_solution(unsigned int i=0);

  /**
   * @returns a reference to one of the system's weighted sensitivity
   * adjoint solution vectors, by default the one corresponding to the
   * first qoi.
   */
  NumericVector<Number> & get_weighted_sensitivity_adjoint_solution(unsigned int i=0);

  /**
   * @returns a reference to one of the system's weighted sensitivity
   * adjoint solution vectors, by default the one corresponding to the
   * first qoi.
   */
  const NumericVector<Number> & get_weighted_sensitivity_adjoint_solution(unsigned int i=0) const;

  /**
   * @returns a reference to the solution of the last weighted
   * sensitivity solve
   * Creates the vector if it doesn't already exist.
   */
  NumericVector<Number> & add_weighted_sensitivity_solution();

  /**
   * @returns a reference to the solution of the last weighted
   * sensitivity solve
   */
  NumericVector<Number> & get_weighted_sensitivity_solution();

  /**
   * @returns a reference to the solution of the last weighted
   * sensitivity solve
   */
  const NumericVector<Number> & get_weighted_sensitivity_solution() const;

  /**
   * @returns a reference to one of the system's adjoint rhs
   * vectors, by default the one corresponding to the first qoi.
   * Creates the vector if it doesn't already exist.
   */
  NumericVector<Number> & add_adjoint_rhs(unsigned int i=0);

  /**
   * @returns a reference to one of the system's adjoint rhs
   * vectors, by default the one corresponding to the first qoi.
   * This what the user's QoI derivative code should assemble
   * when setting up an adjoint problem
   */
  NumericVector<Number> & get_adjoint_rhs(unsigned int i=0);

  /**
   * @returns a reference to one of the system's adjoint rhs
   * vectors, by default the one corresponding to the first qoi.
   */
  const NumericVector<Number> & get_adjoint_rhs(unsigned int i=0) const;

  /**
   * @returns a reference to one of the system's sensitivity rhs
   * vectors, by default the one corresponding to the first parameter.
   * Creates the vector if it doesn't already exist.
   */
  NumericVector<Number> & add_sensitivity_rhs(unsigned int i=0);

  /**
   * @returns a reference to one of the system's sensitivity rhs
   * vectors, by default the one corresponding to the first parameter.
   * By default these vectors are built by the library, using finite
   * differences, when \p assemble_residual_derivatives() is called.
   *
   * When assembled, this vector should hold 
   * -(partial R / partial p_i)
   */
  NumericVector<Number> & get_sensitivity_rhs(unsigned int i=0);

  /**
   * @returns a reference to one of the system's sensitivity rhs
   * vectors, by default the one corresponding to the first parameter.
   */
  const NumericVector<Number> & get_sensitivity_rhs(unsigned int i=0) const;

  /**
   * @returns the number of vectors (in addition to the solution)
   * handled by this system
   * This is the size of the \p _vectors map
   */
  unsigned int n_vectors () const;

  /**
   * @returns the number of variables in the system
   */
  unsigned int n_vars() const;

  /**
   * @returns the number of degrees of freedom in the system
   */
  unsigned int n_dofs() const;

  /**
   * Returns the number of active degrees of freedom
   * for this System.
   */
  unsigned int n_active_dofs() const;

  /**
   * @returns the number of constrained degrees of freedom
   * in the system.
   */
  unsigned int n_constrained_dofs() const;
  
  /**
   * @returns the number of degrees of freedom local
   * to this processor
   */
  unsigned int n_local_dofs() const;

  /**
   * This class defines the notion of a variable in the system.
   * A variable is one of potentially several unknowns in the 
   * problem at hand.  A variable is described by a unique 
   * name, a finite element approximation family, and 
   * (optionally) a list of subdomains to which the 
   * variable is restricted.
   */  
  class Variable
  {
  public:
    
    /**
     * Constructor.  Omits the subdomain mapping, hence this
     * constructor creates a variable which is active on 
     * all subdomains.
     */
    Variable (const std::string &var_name,
	      const unsigned int var_number,
	      const FEType &var_type) :
      _name(var_name),
      _number(var_number),
      _type(var_type)
    {}
    
    /**
     * Constructor.  Takes a set which contains the subdomain
     * indices for which this variable is active.
     */ 
    Variable (const std::string &var_name,
	      const unsigned int var_number,
	      const FEType &var_type,
	      const std::set<subdomain_id_type> &var_active_subdomains) :
      _name(var_name),
      _number(var_number),
      _type(var_type),
      _active_subdomains(var_active_subdomains)
    {}
    
    /**
     * Arbitrary, user-specified name of the variable.
     */
    const std::string & name() const 
    { return _name; }

    /**
     * The rank of this variable in the system.
     */
    unsigned int number() const 
    { return _number; }

    /**
     * The \p FEType for this variable.
     */
    const FEType & type() const 
    { return _type; }

    /**
     * \p returns \p true if this variable is active on subdomain \p sid,
     * \p false otherwise.  Note that we interperet the special case of an 
     * empty \p _active_subdomains container as active everywhere, i.e. 
     * for all subdomains.
     */
    bool active_on_subdomain (const subdomain_id_type sid) const
    { return (_active_subdomains.empty() || _active_subdomains.count(sid));  }

    /**
     * \p returns \p true if this variable is active on all subdomains
     * because it has no specified activity map.  This can be used
     * to perform more efficient computations in some places.
     */
    bool implicitly_active () const
    { return _active_subdomains.empty(); }
    
  private:
    std::string             _name; 
    unsigned int            _number;
    FEType                  _type;
    std::set<subdomain_id_type> _active_subdomains;
  };

  /**
   * Adds the variable \p var to the list of variables
   * for this system.  Returns the index number for the new variable.
   */
  unsigned int add_variable (const std::string& var,
		             const FEType& type,
			     const std::set<subdomain_id_type> * const active_subdomains = NULL);

  /**
   * Adds the variable \p var to the list of variables
   * for this system.  Same as before, but assumes \p LAGRANGE
   * as default value for \p FEType.family.
   */
  unsigned int add_variable (const std::string& var,
		             const Order order = FIRST,
		             const FEFamily = LAGRANGE,
			     const std::set<subdomain_id_type> * const active_subdomains = NULL);

  /** 
   * Return a constant reference to \p Variable \p var.
   */
  const Variable & variable (unsigned int var) const;

  /**
   * @returns true if a variable named \p var exists in this System
   */
  bool has_variable(const std::string& var) const;
  
  /**
   * @returns the name of variable \p i.
   */
  const std::string & variable_name(const unsigned int i) const;
  
  /**
   * @returns the variable number assoicated with
   * the user-specified variable named \p var.
   */
  unsigned short int variable_number (const std::string& var) const;

  /**
   * @returns the finite element type variable number \p i.
   */
  const FEType & variable_type (const unsigned int i) const;

  /**
   * @returns the finite element type for variable \p var.
   */
  const FEType & variable_type (const std::string& var) const;

  /**
   * @returns a norm of variable \p var in the vector \p v, in the specified
   * norm (e.g. L2, H0, H1)
   */
  Real calculate_norm(NumericVector<Number>& v,
		      unsigned int var = 0,
		      FEMNormType norm_type = L2) const;

  /**
   * @returns a norm of the vector \p v, using \p component_norm and \p
   * component_scale to choose and weight the norms of each variable.
   */
  Real calculate_norm(NumericVector<Number>& v,
		      const SystemNorm &norm) const;

  /**
   * Reads the basic data header for this System.
   */
  void read_header (Xdr& io,
		    const std::string &version,
		    const bool read_header=true,
		    const bool read_additional_data=true,
		    const bool read_legacy_format=false);

  /**
   * Reads additional data, namely vectors, for this System.
   */
  void read_legacy_data (Xdr& io,
			 const bool read_additional_data=true);

  /**
   * Reads additional data, namely vectors, for this System.
   * This method may safely be called on a distributed-memory mesh.
   */
  void read_serialized_data (Xdr& io,
			     const bool read_additional_data=true);

  /**
   * Reads additional data, namely vectors, for this System.
   * This method may safely be called on a distributed-memory mesh.
   * This method will read an individual file for each processor in the simulation
   * where the local solution components for that processor are stored.
   */
  void read_parallel_data (Xdr &io,
			   const bool read_additional_data);
  /**
   * Writes the basic data header for this System.
   */
  void write_header (Xdr& io,
		     const std::string &version,
		     const bool write_additional_data) const;

  /**
   * Writes additional data, namely vectors, for this System.
   * This method may safely be called on a distributed-memory mesh.
   */
  void write_serialized_data (Xdr& io,
			      const bool write_additional_data = true) const;

  /**
   * Writes additional data, namely vectors, for this System.
   * This method may safely be called on a distributed-memory mesh.
   * This method will create an individual file for each processor in the simulation
   * where the local solution components for that processor will be stored.
   */
  void write_parallel_data (Xdr &io,
			    const bool write_additional_data) const;
  
  /**
   * @returns a string containing information about the
   * system.
   */
  std::string get_info () const;

  /**
   * Register a user function to use in initializing the system.
   */
  void attach_init_function (void fptr(EquationSystems& es,
				       const std::string& name));
  
  /**
   * Register a user function to use in assembling the system
   * matrix and RHS.
   */
  void attach_assemble_function (void fptr(EquationSystems& es,
					   const std::string& name));
  
  /**
   * Register a user function for imposing constraints.
   */
  void attach_constraint_function (void fptr(EquationSystems& es,
					     const std::string& name));
  
  /**
   * Register a user function for evaluating the quantities of interest,
   * whose values should be placed in \p System::qoi
   */
  void attach_QOI_function (void fptr(EquationSystems& es,
				      const std::string& name,
                                      const QoISet& qoi_indices));
  
  /**
   * Register a user function for evaluating derivatives of a quantity
   * of interest with respect to test functions, whose values should
   * be placed in \p System::rhs
   */
  void attach_QOI_derivative (void fptr(EquationSystems& es,
				        const std::string& name,
                                        const QoISet& qoi_indices));
  
  /**
   * Calls user's attached initialization function, or is overloaded by
   * the user in derived classes.
   */
  virtual void user_initialization ();
  
  /**
   * Calls user's attached assembly function, or is overloaded by
   * the user in derived classes.
   */
  virtual void user_assembly ();
  
  /**
   * Calls user's attached constraint function, or is overloaded by
   * the user in derived classes.
   */
  virtual void user_constrain ();

  /**
   * Calls user's attached quantity of interest function, or is
   * overloaded by the user in derived classes.
   */
  virtual void user_QOI (const QoISet& qoi_indices);

  /**
   * Calls user's attached quantity of interest derivative function,
   * or is overloaded by the user in derived classes.
   */
  virtual void user_QOI_derivative (const QoISet& qoi_indices);

  /**
   * Re-update the local values when the mesh has changed.
   * This method takes the data updated by \p update() and
   * makes it up-to-date on the current mesh.
   */
  virtual void re_update ();

  /**
   * Restrict vectors after the mesh has coarsened
   */
  virtual void restrict_vectors ();

  /**
   * Prolong vectors after the mesh has refined
   */
  virtual void prolong_vectors ();

  /**
   * Flag which tells the system to whether or not to
   * call the user assembly function during each call to solve().
   * By default, every call to solve() begins with a call to the
   * user assemble, so this flag is true.  (For explicit systems,
   * "solving" the system occurs during the assembly step, so this
   * flag is always true for explicit systems.)  
   * 
   * You will only want to set this to false if you need direct
   * control over when the system is assembled, and are willing to
   * track the state of its assembly yourself.  An example of such a
   * case is an implicit system with multiple right hand sides.  In
   * this instance, a single assembly would likely be followed with
   * multiple calls to solve.
   *
   * The frequency system and Newmark system have their own versions
   * of this flag, called _finished_assemble, which might be able to
   * be replaced with this more general concept.
   */
  bool assemble_before_solve;


  //--------------------------------------------------
  // The solution and solution access members
  
  /**
   * @returns the current solution for the specified global
   * DOF.
   */
  Number current_solution (const unsigned int global_dof_number) const;

  /**
   * Data structure to hold solution values.
   */
  AutoPtr<NumericVector<Number> > solution;

  /**
   * All the values I need to compute my contribution
   * to the simulation at hand.  Think of this as the
   * current solution with any ghost values needed from
   * other processors.  This vector is necessarily larger
   * than the \p solution vector in the case of a parallel
   * simulation.  The \p update() member is used to synchronize
   * the contents of the \p solution and \p current_local_solution
   * vectors.
   */
  AutoPtr<NumericVector<Number> > current_local_solution;

  /**
   * Values of the quantities of interest.  This vector needs
   * to be both resized and filled by the user before any quantity of
   * interest assembly is done and before any sensitivities are
   * calculated.
   */
  std::vector<Number> qoi;

  /**
   * Fills the std::set with the degrees of freedom on the local
   * processor corresponding the the variable number passed in.
   */
  void local_dof_indices (const unsigned int var,
                          std::set<unsigned int> & var_indices) const;

  /**
   * Zeroes all dofs in \p v that correspond to variable number \p
   * var_num.
   */
  void zero_variable (NumericVector<Number>& v, unsigned int var_num) const;

protected:

  /**
   * Initializes the data for the system.  Note that this is called
   * before any user-supplied intitialization function so that all
   * required storage will be available.
   */
  virtual void init_data ();

  /**
   * Projects the vector defined on the old mesh onto the
   * new mesh. 
   */
  void project_vector (NumericVector<Number>&) const;

  /**
   * Projects the vector defined on the old mesh onto the
   * new mesh. The original vector is unchanged and the new vector
   * is passed through the second argument.
   */
  void project_vector (const NumericVector<Number>&,
		       NumericVector<Number>&) const;
  
private:

  /**
   * Finds the discrete norm for the entries in the vector
   * corresponding to Dofs associated with var.
   */
  Real discrete_var_norm(NumericVector<Number>& v,
                         unsigned int var,
                         FEMNormType norm_type) const;

  /**
   * Reads an input vector from the stream \p io and assigns
   * the values to a set of \p DofObjects.  This method uses
   * blocked input and is safe to call on a distributed memory-mesh.
   */   
  template <typename iterator_type>
  unsigned int read_serialized_blocked_dof_objects (const unsigned int var,
						    const unsigned int n_objects,
						    const iterator_type begin,
						    const iterator_type end,
						    Xdr &io,
						    NumericVector<Number> &vec) const;

  /**
   * Reads a vector for this System.
   * This method may safely be called on a distributed-memory mesh.
   */
  void read_serialized_vector (Xdr& io,
			       NumericVector<Number> &vec);

  /**
   * Writes an output vector to the stream \p io for a set of \p DofObjects.
   * This method uses blocked output and is safe to call on a distributed memory-mesh.
   */   
  template <typename iterator_type>
  unsigned int write_serialized_blocked_dof_objects (const NumericVector<Number> &vec,
						     const unsigned int var,
						     const unsigned int n_objects,
						     const iterator_type begin,
						     const iterator_type end,
						     Xdr &io) const;

  /**
   * Writes a vector for this System.
   * This method may safely be called on a distributed-memory mesh.
   */
  void write_serialized_vector (Xdr& io,
				const NumericVector<Number> &vec) const;

  /**
   * Function that initializes the system.
   */
  void (* _init_system) (EquationSystems& es,
			 const std::string& name);
  
  /**
   * Function that assembles the system.
   */
  void (* _assemble_system) (EquationSystems& es,
			     const std::string& name);

  /**
   * Function to impose constraints.
   */
  void (* _constrain_system) (EquationSystems& es, 
			      const std::string& name);

  /**
   * Function to evaluate quantity of interest
   */
  void (* _qoi_evaluate) (EquationSystems& es, 
			  const std::string& name,
                          const QoISet& qoi_indices);

  /**
   * Function to evaluate quantity of interest derivative
   */
  void (* _qoi_evaluate_derivative) (EquationSystems& es, 
			             const std::string& name,
                                     const QoISet& qoi_indices);

  /**
   * Data structure describing the relationship between
   * nodes, variables, etc... and degrees of freedom.
   */
  AutoPtr<DofMap> _dof_map;

  /**
   * Constant reference to the \p EquationSystems object
   * used for the simulation.
   */
  EquationSystems& _equation_systems;
  
  /**
   * Constant reference to the \p mesh data structure used
   * for the simulation.   
   */
  MeshBase& _mesh;
  
  /**
   * A name associated with this system.
   */
  const std::string _sys_name;

  /**
   * The number associated with this system
   */
  const unsigned int _sys_number;
  
  /**
   * The \p Variables in this \p System.
   */
  std::vector<System::Variable> _variables;

  /**
   * The variable numbers corresponding to user-specified
   * names, useful for name-based lookups.
   */
  std::map<std::string, unsigned short int> _variable_numbers;

  /**
   * Flag stating if the system is active or not.
   */
  bool _active;
  
  /**
   * Some systems need an arbitrary number of vectors.
   * This map allows names to be associated with arbitrary
   * vectors.  All the vectors in this map will be distributed
   * in the same way as the solution vector.
   */
  std::map<std::string, NumericVector<Number>* > _vectors;

  /**
   * Holds true if a vector by that name should be projected
   * onto a changed grid, false if it should be zeroed.
   */
  std::map<std::string, bool> _vector_projections;

  /**
   * Holds true if the solution vector should be projected
   * onto a changed grid, false if it should be zeroed.
   * This is true by default.
   */
  bool _solution_projection;

  /**
   * \p true when additional vectors may still be added, \p false otherwise.
   */
  bool _can_add_vectors;

  /**
   * This flag is used only when *reading* in a system from file.
   * Based on the system header, it keeps track of whether or not
   * additional vectors were actually written for this file.
   */
  bool _additional_data_written;


  /**
   * This class implements projecting a vector from 
   * an old mesh to the newly refined mesh.  This
   * may be executed in parallel on multiple threads.
   */
  class ProjectVector 
  {
  private:
    const System                &system;
    const NumericVector<Number> &old_vector;
    NumericVector<Number>       &new_vector;

  public:
    ProjectVector (const System &system_in,
		   const NumericVector<Number> &old_v_in,
		   NumericVector<Number> &new_v_in) :
    system(system_in),
    old_vector(old_v_in),
    new_vector(new_v_in)
    {}
    
    void operator()(const ConstElemRange &range) const;
  };


  /**
   * This class builds the send_list of old dof indices
   * whose coefficients are needed to perform a projection.
   * This may be executed in parallel on multiple threads.
   * The end result is a \p send_list vector which is 
   * unsorted and may contain duplicate elements.
   * The \p unique() method can be used to sort and
   * create a unique list.
   */
  class BuildProjectionList
  {
  private:
    const System              &system;

  public:
    BuildProjectionList (const System &system_in) :
    system(system_in)
    {}

    BuildProjectionList (BuildProjectionList &other, Threads::split) :
     system(other.system)
    {}
    
    void unique();
    void operator()(const ConstElemRange &range);
    void join (const BuildProjectionList &other);
    std::vector<unsigned int> send_list;
  };



  /**
   * This class implements projecting a vector from 
   * an old mesh to the newly refined mesh.  This
   * may be exectued in parallel on multiple threads.
   */
  class ProjectSolution
  {
  private:
    const System                &system;

    Number (* fptr)(const Point& p,
		    const Parameters& parameters,
		    const std::string& sys_name,
		    const std::string& unknown_name);

    Gradient (* gptr)(const Point& p,
		      const Parameters& parameters,
		      const std::string& sys_name,
		      const std::string& unknown_name);

    Parameters &parameters;
    NumericVector<Number>       &new_vector;

  public:
    ProjectSolution (const System &system_in,
		     Number fptr_in(const Point& p,
				    const Parameters& parameters,
				    const std::string& sys_name,
				    const std::string& unknown_name),
		     Gradient gptr_in(const Point& p,
				      const Parameters& parameters,
				      const std::string& sys_name,
				      const std::string& unknown_name),
		     Parameters &parameters_in,
		     NumericVector<Number> &new_v_in) :
    system(system_in),
    fptr(fptr_in),
    gptr(gptr_in),
    parameters(parameters_in),
    new_vector(new_v_in)
    {}
    
    void operator()(const ConstElemRange &range) const;
  };
};



// ------------------------------------------------------------
// System inline methods
inline
const std::string & System::name() const
{
  return _sys_name;
}



inline
unsigned int System::number() const
{
  return _sys_number;
}



inline
const MeshBase & System::get_mesh() const
{
  return _mesh;
}



inline
MeshBase & System::get_mesh()
{
  return _mesh;
}



inline
const DofMap & System::get_dof_map() const
{
  return *_dof_map;
}



inline
DofMap & System::get_dof_map()
{
  return *_dof_map;
}



inline
bool System::active() const
{
  return _active;  
}



inline
void System::activate ()
{
  _active = true;
}



inline
void System::deactivate ()
{
  _active = false;
}



inline
unsigned int System::n_vars() const
{
  return _variables.size();
}



inline
const System::Variable & System::variable (const unsigned int i) const
{
  libmesh_assert (i < _variables.size());

  return _variables[i];
}



inline
const std::string & System::variable_name (const unsigned int i) const
{
  libmesh_assert (i < _variables.size());

  return _variables[i].name();
}



inline
const FEType & System::variable_type (const unsigned int i) const
{
  libmesh_assert (i < _variables.size());
  
  return _variables[i].type();
}



inline
const FEType & System::variable_type (const std::string& var) const
{
  return _variables[this->variable_number(var)].type();
}



inline
unsigned int System::n_active_dofs() const
{
  return this->n_dofs() - this->n_constrained_dofs();
}



inline
bool System::have_vector (const std::string& vec_name) const
{
  return (_vectors.count(vec_name));
}



inline
unsigned int System::n_vectors () const
{
  return _vectors.size(); 
}

inline
System::vectors_iterator System::vectors_begin ()
{
  return _vectors.begin();
}

inline
System::const_vectors_iterator System::vectors_begin () const
{
  return _vectors.begin();
}

inline
System::vectors_iterator System::vectors_end ()
{
  return _vectors.end();
}

inline
System::const_vectors_iterator System::vectors_end () const
{
  return _vectors.end();
}

inline
void System::assemble_residual_derivatives (const ParameterVector&)
{
  libmesh_not_implemented();
}

inline
std::pair<unsigned int, Real>
System::sensitivity_solve (const ParameterVector&)
{
  libmesh_not_implemented();
}

inline
std::pair<unsigned int, Real>
System::weighted_sensitivity_solve (const ParameterVector&,
                                    const ParameterVector&)
{
  libmesh_not_implemented();
}

inline
std::pair<unsigned int, Real>
System::adjoint_solve (const QoISet&)
{
  libmesh_not_implemented();
}

inline
std::pair<unsigned int, Real>
System::weighted_sensitivity_adjoint_solve (const ParameterVector&,
                                            const ParameterVector&,
                                            const QoISet&)
{
  libmesh_not_implemented();
}

inline
void
System::adjoint_qoi_parameter_sensitivity (const QoISet&,
                                           const ParameterVector&,
                                           SensitivityData&)
{
  libmesh_not_implemented();
}

inline
void
System::forward_qoi_parameter_sensitivity (const QoISet&,
                                           const ParameterVector&,
                                           SensitivityData&)
{
  libmesh_not_implemented();
}

inline
void
System::qoi_parameter_hessian(const QoISet&,
                              const ParameterVector&,
                              SensitivityData&)
{
  libmesh_not_implemented();
}

inline
void
System::qoi_parameter_hessian_vector_product(const QoISet&,
                                             const ParameterVector&,
                                             const ParameterVector&,
                                             SensitivityData&)
{
  libmesh_not_implemented();
}

#endif // #define __system_h__