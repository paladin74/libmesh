#include "libmesh/libmesh_config.h"

#ifdef LIBMESH_HAVE_DTK

#include "libmesh/dtk_solution_transfer.h"

#include "libmesh/system.h"

// Trilinos Includes
#include <Teuchos_RCP.hpp>
#include <Teuchos_GlobalMPISession.hpp>
#include <Teuchos_Ptr.hpp>

// DTK Includes
#include <DTK_MeshManager.hpp>
#include <DTK_MeshContainer.hpp>
#include <DTK_FieldManager.hpp>
#include <DTK_FieldContainer.hpp>
#include <DTK_FieldTools.hpp>
#include <DTK_CommTools.hpp>
#include <DTK_CommIndexer.hpp>

namespace libMesh {

DTKSolutionTransfer::DTKSolutionTransfer()
{
  comm_default = Teuchos::DefaultComm<int>::getComm();
}

DTKSolutionTransfer::~DTKSolutionTransfer()
{
  for(std::map<EquationSystems *, DTKAdapter *>::iterator it = adapters.begin();
      it != adapters.end();
      ++it)
    delete it->second;

  for(std::map<std::pair<EquationSystems *, EquationSystems *>, shared_domain_map_type * >::iterator it = dtk_maps.begin();
      it != dtk_maps.end();
      ++it)
    delete it->second;
}

void
DTKSolutionTransfer::transfer(const Variable & from_var, const Variable & to_var)
{
  EquationSystems * from_es = &from_var.sys()->get_equation_systems();
  EquationSystems * to_es = &to_var.sys()->get_equation_systems();

  // Possibly make an Adapter for from_es
  if(adapters.find(from_es) == adapters.end())
    adapters[from_es] = new DTKAdapter(comm_default, *from_es);
  
  // Possibly make an Adapter for to_es
  if(adapters.find(to_es) == adapters.end())
    adapters[to_es] = new DTKAdapter(comm_default, *to_es);  

  DTKAdapter * from_adapter = adapters[from_es];
  DTKAdapter * to_adapter = adapters[to_es];

  std::pair<EquationSystems *, EquationSystems *> from_to(from_es, to_es);

  // If we haven't created a map for this pair of EquationSystems yet, do it now
  if(dtk_maps.find(from_to) == dtk_maps.end())
  {
    libmesh_assert(from_es->get_mesh().mesh_dimension() == to_es->get_mesh().mesh_dimension());
    
    shared_domain_map_type * map = new shared_domain_map_type(comm_default, from_es->get_mesh().mesh_dimension(), true);
    dtk_maps[from_to] = map;

    // The tolerance here is for the "contains_point()" implementation in DTK.  Set a larger value for a looser tolerance...
    map->setup(from_adapter->get_mesh_manager(), to_adapter->get_target_coords(), 30*Teuchos::ScalarTraits<double>::eps());
  }

  DTKAdapter::RCP_Evaluator from_evaluator = from_adapter->get_variable_evaluator(from_var.name());
  Teuchos::RCP<DataTransferKit::FieldManager<DTKAdapter::FieldContainerType> > to_values = to_adapter->get_values_to_fill(to_var.name());
  
  dtk_maps[from_to]->apply(from_evaluator, to_values);

  if(dtk_maps[from_to]->getMissedTargetPoints().size())
    libMesh::out<<"Warning: Some points were missed in the transfer of "<<from_var.name()<<" to "<<to_var.name()<<"!"<<std::endl;

  to_adapter->update_variable_values(to_var.name());
}

} // namespace libMesh

#endif
