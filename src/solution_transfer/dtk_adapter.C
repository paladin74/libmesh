#include "libmesh/libmesh_config.h"

#ifdef LIBMESH_HAVE_DTK

#include "libmesh/dtk_adapter.h"

#include "libmesh/dtk_evaluator.h"
#include "libmesh/mesh.h"
#include "libmesh/numeric_vector.h"
#include "libmesh/elem.h"

#include <DTK_MeshTypes.hpp>
#include <Teuchos_Comm.hpp>

#include <vector>

namespace libMesh
{

DTKAdapter::DTKAdapter(Teuchos::RCP<const Teuchos::Comm<int> > in_comm, EquationSystems & in_es):
    comm(in_comm),
    es(in_es),
    mesh(in_es.get_mesh()),
    dim(mesh.mesh_dimension())
{
  std::set<unsigned int> semi_local_nodes;
  get_semi_local_nodes(semi_local_nodes);

  num_local_nodes = semi_local_nodes.size();
  
  vertices.resize(num_local_nodes);
  Teuchos::ArrayRCP<double> coordinates(num_local_nodes * dim);

  // Fill in the vertices and coordinates
  {
    unsigned int i = 0;

    for(std::set<unsigned int>::iterator it = semi_local_nodes.begin();
        it != semi_local_nodes.end();
        ++it)
    {
      const Node & node = mesh.node(*it);
      
      vertices[i] = node.id();
      
      for(unsigned int j=0; j<dim; j++)
        coordinates[(j*num_local_nodes) + i] = node(j);
          
      i++;
    }
  }
  
  // Currently assuming all elements are the same!
  DataTransferKit::DTK_ElementTopology element_topology = get_element_topology(mesh.elem(0));
  unsigned int n_nodes_per_elem = mesh.elem(0)->n_nodes();

  Teuchos::ArrayRCP<int> elements(mesh.n_local_elem());
  Teuchos::ArrayRCP<int> connectivity(n_nodes_per_elem*mesh.n_local_elem());

  // Fill in the elements and connectivity
  {
    unsigned int i = 0;

    MeshBase::const_element_iterator end = mesh.local_elements_end();
    for(MeshBase::const_element_iterator it = mesh.local_elements_begin();
        it != end;
        ++it)
    {
      const Elem & elem = *(*it);
      elements[i] = elem.id();

      for(unsigned int j=0; j<n_nodes_per_elem; j++)
        connectivity[(j*mesh.n_local_elem())+i] = elem.node(j);
          
      i++;
    }
  }

  Teuchos::ArrayRCP<int> permutation_list(n_nodes_per_elem);
  for ( int i = 0; i < n_nodes_per_elem; ++i )
    permutation_list[i] = i;

  /*
  if(libMesh::processor_id() == 1)
    sleep(1);
    
  std::cout<<"n_nodes_per_elem: "<<n_nodes_per_elem<<std::endl;

  std::cout<<"Dim: "<<dim<<std::endl;

  std::cerr<<"Vertices size: "<<vertices.size()<<std::endl;
  {
    std::cerr<<libMesh::processor_id()<<" Vertices: ";
    
    for(unsigned int i=0; i<vertices.size(); i++)
      std::cerr<<vertices[i]<<" ";

    std::cerr<<std::endl;
  }

  std::cerr<<"Coordinates size: "<<coordinates.size()<<std::endl;
  {
    std::cerr<<libMesh::processor_id()<<" Coordinates: ";
    
    for(unsigned int i=0; i<coordinates.size(); i++)
      std::cerr<<coordinates[i]<<" ";

    std::cerr<<std::endl;
  }

  std::cerr<<"Connectivity size: "<<connectivity.size()<<std::endl;
  {
    std::cerr<<libMesh::processor_id()<<" Connectivity: ";
    
    for(unsigned int i=0; i<connectivity.size(); i++)
      std::cerr<<connectivity[i]<<" ";

    std::cerr<<std::endl;
  }

  std::cerr<<"Permutation_List size: "<<permutation_list.size()<<std::endl;
  {
    std::cerr<<libMesh::processor_id()<<" Permutation_List: ";
    
    for(unsigned int i=0; i<permutation_list.size(); i++)
      std::cerr<<permutation_list[i]<<" ";

    std::cerr<<std::endl;
  }
  
  */
  Teuchos::RCP<MeshContainerType> mesh_container = Teuchos::rcp(
    new MeshContainerType(dim, vertices, coordinates, 
                          element_topology, n_nodes_per_elem, 
                          elements, connectivity, permutation_list) );

  // We only have 1 element topology in this grid so we make just one mesh block
  Teuchos::ArrayRCP<Teuchos::RCP<MeshContainerType> > mesh_blocks(1);
  mesh_blocks[0] = mesh_container;

  // Create the MeshManager
  mesh_manager = Teuchos::rcp(new DataTransferKit::MeshManager<MeshContainerType>(mesh_blocks, comm, dim) );

  // Pack the coordinates into a field, this will be the positions we'll ask for other systems fields at
  target_coords = Teuchos::rcp(new DataTransferKit::FieldManager<MeshContainerType>(mesh_container, comm));
}

DTKAdapter::RCP_Evaluator
DTKAdapter::get_variable_evaluator(std::string var_name)
{
  if(evaluators.find(var_name) == evaluators.end()) // We haven't created an evaluator for the variable yet
  {
    System * sys = find_sys(var_name);
    
    // Create the FieldEvaluator
    evaluators[var_name] = Teuchos::rcp(new DTKEvaluator(*sys, var_name));
  }
  
  return evaluators[var_name];
}

Teuchos::RCP<DataTransferKit::FieldManager<DTKAdapter::FieldContainerType> >
DTKAdapter::get_values_to_fill(std::string var_name)
{
  if(values_to_fill.find(var_name) == values_to_fill.end())
  {
    Teuchos::ArrayRCP<double> data_space(num_local_nodes);
    Teuchos::RCP<FieldContainerType> field_container = Teuchos::rcp(new FieldContainerType(data_space, 1));
    values_to_fill[var_name] = Teuchos::rcp(new DataTransferKit::FieldManager<FieldContainerType>(field_container, comm));
  }

  return values_to_fill[var_name];
}

void
DTKAdapter::update_variable_values(std::string var_name)
{
  System * sys = find_sys(var_name);
  unsigned int var_num = sys->variable_number(var_name);
  
  Teuchos::RCP<FieldContainerType> values = values_to_fill[var_name]->field();

  unsigned int i=0;  
  // Loop over the values (one for each node) and assign the value of this variable at each node
  for(FieldContainerType::iterator it=values->begin(); it != values->end(); ++it)
  {
    unsigned int node_num = vertices[i];
    const Node & node = mesh.node(node_num);

    if(node.processor_id() == libMesh::processor_id())
    { 
      // The 0 is for the component... this only works for LAGRANGE!
      dof_id_type dof = node.dof_number(sys->number(), var_num, 0);
      sys->solution->set(dof, *it);
    }

    i++;
  }

  sys->solution->close();
}


/**
 * Small helper function for finding the system containing the variable.
 *
 * Note that this implies that variable names are unique across all systems!
 */
System *
DTKAdapter::find_sys(std::string var_name)
{
  System * sys = NULL;
  
  // Find the system this variable is from
  for(unsigned int i=0; i<es.n_systems(); i++)
  {
    if(es.get_system(i).has_variable(var_name))
    {
      sys = &es.get_system(i);
      break;
    }
  }
  
  libmesh_assert(sys);
  
  return sys;
}

DataTransferKit::DTK_ElementTopology
DTKAdapter::get_element_topology(const Elem * elem)
{
  ElemType type = elem->type();

  if(type == EDGE2)
    return DataTransferKit::DTK_LINE_SEGMENT;
  else if(type == TRI3)
    return DataTransferKit::DTK_TRIANGLE;
  else if(type == QUAD4)
    return DataTransferKit::DTK_QUADRILATERAL;
  else if(type == TET4)
    return DataTransferKit::DTK_TETRAHEDRON;
  else if(type == HEX8)
    return DataTransferKit::DTK_HEXAHEDRON;
  else if(type == PYRAMID5)
    return DataTransferKit::DTK_PYRAMID;
  
  std::cout<<"Element type not supported by DTK!"<<std::endl;
  libmesh_error();
}

void
DTKAdapter::get_semi_local_nodes(std::set<unsigned int> & semi_local_nodes)
{
  MeshBase::const_element_iterator end = mesh.local_elements_end();
  for(MeshBase::const_element_iterator it = mesh.local_elements_begin();
      it != end;
      ++it)
  {
    const Elem & elem = *(*it);

    for(unsigned int j=0; j<elem.n_nodes(); j++)
      semi_local_nodes.insert(elem.node(j));
  }
}

} // namespace libMesh

#endif
