// The libMesh Finite Element Library.
// Copyright (C) 2002-2012 Benjamin S. Kirk, John W. Peterson, Roy H. Stogner

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


// C++ includes
#include <fstream>

// Local includes
#include "libmesh/libmesh_config.h"
#include "libmesh/vtk_io.h"
#include "libmesh/mesh_base.h"
#include "libmesh/equation_systems.h"
#include "libmesh/cell_tet4.h"
#include "libmesh/cell_tet10.h"
#include "libmesh/cell_prism6.h"
#include "libmesh/cell_pyramid5.h"
#include "libmesh/cell_hex8.h"
#include "libmesh/cell_hex20.h"
#include "libmesh/numeric_vector.h"
#include "libmesh/system.h"
#include "libmesh/mesh_data.h"

#ifdef LIBMESH_HAVE_VTK

#include "libmesh/ignore_warnings.h"
#include "vtkXMLUnstructuredGridReader.h"
#include "vtkXMLUnstructuredGridWriter.h"
#include "vtkXMLPUnstructuredGridWriter.h"
#include "vtkUnstructuredGrid.h"
#include "vtkGenericGeometryFilter.h"
#include "vtkCellArray.h"
#include "vtkConfigure.h"
#include "vtkDoubleArray.h"
#include "vtkPointData.h"
#include "vtkPoints.h"
#include "vtkSmartPointer.h"
#include "libmesh/restore_warnings.h"

// A convenient macro for comparing VTK versions.  Returns 1 if the
// current VTK version is < major.minor.subminor and zero otherwise.
//
// It relies on the VTK version numbers detected during configure.  Note that if
// LIBMESH_HAVE_VTK is not defined, none of the LIBMESH_DETECTED_VTK_VERSION_* variables will
// be defined either.
#define VTK_VERSION_LESS_THAN(major,minor,subminor)                                                     \
  ((LIBMESH_DETECTED_VTK_VERSION_MAJOR < (major) ||                                                     \
    (LIBMESH_DETECTED_VTK_VERSION_MAJOR == (major) && (LIBMESH_DETECTED_VTK_VERSION_MINOR < (minor) ||  \
                                  (LIBMESH_DETECTED_VTK_VERSION_MINOR == (minor) &&                     \
                                   LIBMESH_DETECTED_VTK_VERSION_SUBMINOR < (subminor))))) ? 1 : 0)




#endif //LIBMESH_HAVE_VTK

namespace libMesh
{

#ifdef LIBMESH_HAVE_VTK // private functions
vtkIdType VTKIO::get_elem_type(ElemType type) {
  vtkIdType celltype = VTK_EMPTY_CELL; // initialize to something to avoid compiler warning

  switch(type)
  {
    case EDGE2:
      celltype = VTK_LINE;
      break;
    case EDGE3:
      celltype = VTK_QUADRATIC_EDGE;
      break;// 1
    case TRI3:
      celltype = VTK_TRIANGLE;
      break;// 3
    case TRI6:
      celltype = VTK_QUADRATIC_TRIANGLE;
      break;// 4
    case QUAD4:
      celltype = VTK_QUAD;
      break;// 5
    case QUAD8:
      celltype = VTK_QUADRATIC_QUAD;
      break;// 6
    case TET4:
      celltype = VTK_TETRA;
      break;// 8
    case TET10:
      celltype = VTK_QUADRATIC_TETRA;
      break;// 9
    case HEX8:
      celltype = VTK_HEXAHEDRON;
      break;// 10
    case HEX20:
      celltype = VTK_QUADRATIC_HEXAHEDRON;
      break;// 12
    case HEX27:
      celltype = VTK_TRIQUADRATIC_HEXAHEDRON;
      break;
    case PRISM6:
      celltype = VTK_WEDGE;
      break;// 13
    case PRISM15:
      celltype = VTK_QUADRATIC_WEDGE;
      break;// 14
    case PRISM18:
      celltype = VTK_BIQUADRATIC_QUADRATIC_WEDGE;
      break;// 15
    case PYRAMID5:
      celltype = VTK_PYRAMID;
      break;// 16
#if VTK_MAJOR_VERSION > 5 || (VTK_MAJOR_VERSION == 5 && VTK_MINOR_VERSION > 0)
    case QUAD9:
      celltype = VTK_BIQUADRATIC_QUAD;
      break;
#else
    case QUAD9:
#endif
    case EDGE4:
    case INFEDGE2:
    case INFQUAD4:
    case INFQUAD6:
    case INFHEX8:
    case INFHEX16:
    case INFHEX18:
    case INFPRISM6:
    case INFPRISM12:
    case NODEELEM:
    case INVALID_ELEM:
    default:
      {
        libMesh::err<<"element type "<<type<<" not implemented"<<std::endl;
        libmesh_error();
        break;
      }
  }
  return celltype;
}


void VTKIO::nodes_to_vtk()
{
  const MeshBase& mesh = MeshOutput<MeshBase>::mesh();

  // containers for points and coordinates of points
  vtkSmartPointer<vtkPoints> points = vtkPoints::New();
  vtkSmartPointer<vtkDoubleArray> pcoords = vtkDoubleArray::New();
  pcoords->SetNumberOfComponents(LIBMESH_DIM);
  points->SetNumberOfPoints(mesh.n_local_nodes()); // it seems that it needs this to prevent a segfault
 
  unsigned int local_node_counter = 0;

  MeshBase::const_node_iterator nd = mesh.local_nodes_begin();
  MeshBase::const_node_iterator nd_end = mesh.local_nodes_end();
  for (; nd != nd_end; nd++, local_node_counter++)
    {
      Node* node = (*nd);

      float pnt[LIBMESH_DIM];
      for (unsigned int i = 0; i < LIBMESH_DIM; ++i)
        pnt[i] = (*node)(i);

      // Fill mapping between global and local node numbers
      _local_node_map[node->id()] = local_node_counter;

      // add point
      pcoords->InsertNextTuple(pnt); // SetTuple(node->id(),pnt);
    }

  // add coordinates to points
  points->SetData(pcoords);
  pcoords->Delete();
  // add points to grid
  _vtk_grid->SetPoints(points);
  points->Delete();
}


void VTKIO::cells_to_vtk()
{
  const MeshBase& mesh = MeshOutput<MeshBase>::mesh();

  vtkSmartPointer<vtkCellArray> cells = vtkCellArray::New();
  vtkSmartPointer<vtkIdList> pts = vtkIdList::New();

  std::vector<int> types(mesh.n_active_local_elem());
  int active_element_counter = 0;

  MeshBase::const_element_iterator it = mesh.active_local_elements_begin();
  const MeshBase::const_element_iterator end = mesh.active_local_elements_end();
  for (; it != end; it++, ++active_element_counter)
    {
      Elem *elem = *it;

      pts->SetNumberOfIds(elem->n_nodes());

      // get the connectivity for this element
      std::vector<unsigned int> conn;
      elem->connectivity(0, VTK, conn);

      for (unsigned int i = 0; i < conn.size(); ++i)
        {
          if (_local_node_map.find(conn[i]) == _local_node_map.end())
            {
              // Ghost node
              float pt[LIBMESH_DIM];
              unsigned int node = elem->node(i);
              for (unsigned int d = 0; d < LIBMESH_DIM; ++d)
                {
                pt[d] = mesh.node(node)(d);
                }
              vtkIdType local = _vtk_grid->GetPoints()->InsertNextPoint(pt);
              _local_node_map[node] = local;
            }
          pts->InsertId(i, _local_node_map[conn[i]]);
        }

      cells->InsertNextCell(pts);
      types[active_element_counter] = this->get_elem_type(elem->type());
    } // end loop over active elements
  pts->Delete();
  _vtk_grid->SetCells(&types[0], cells);
  cells->Delete();
}

/*
 * FIXME now this is known to write nonsense on AMR meshes
 * and it strips the imaginary parts of complex Numbers
 */
void VTKIO::system_vectors_to_vtk(const EquationSystems& es,vtkUnstructuredGrid*& grid){
	if (libMesh::processor_id() == 0){

		std::map<std::string,std::vector<Number> > vecs;
		for(unsigned int i=0;i<es.n_systems();++i){
			const System& sys = es.get_system(i);
			System::const_vectors_iterator v_end = sys.vectors_end();
			System::const_vectors_iterator it = sys.vectors_begin();
			for(;it!= v_end;++it){ // for all vectors on this system
				std::vector<Number> values;
//				libMesh::out<<"it "<<it->first<<std::endl;

				it->second->localize_to_one(values,0);
//				libMesh::out<<"finish localize"<<std::endl;
				vecs[it->first] = values;
			}
		}


		std::map<std::string,std::vector<Number> >::iterator it = vecs.begin();

		for(;it!=vecs.end();++it){

			vtkDoubleArray *data = vtkDoubleArray::New();

			data->SetName(it->first.c_str());

			libmesh_assert_equal_to (it->second.size(), es.get_mesh().n_nodes());

			data->SetNumberOfValues(it->second.size());

			for(unsigned int i=0;i<it->second.size();++i){

#ifdef LIBMESH_USE_COMPLEX_NUMBERS
				data->SetValue(i,it->second[i].real());
#else
				data->SetValue(i,it->second[i]);
#endif

			}

			grid->GetPointData()->AddArray(data);
			data->Delete();
		}

	}

}

/*
// write out mesh data to the VTK file, this might come in handy to display
// boundary conditions and material data
inline void meshdata_to_vtk(const MeshData& meshdata,
										vtkUnstructuredGrid* grid){
	vtkPointData* pointdata = vtkPointData::New();

	const unsigned int n_vn = meshdata.n_val_per_node();
	const unsigned int n_dat = meshdata.n_node_data();

	pointdata->SetNumberOfTuples(n_dat);
}*/

#endif


// ------------------------------------------------------------
// vtkIO class members
//
void VTKIO::read (const std::string& name)
{
  // This is a serial-only process for now;
  // the Mesh should be read on processor 0 and
  // broadcast later
  libmesh_assert_equal_to (libMesh::processor_id(), 0);

  // Keep track of what kinds of elements this file contains
  elems_of_dimension.clear();
  elems_of_dimension.resize(4, false);

#ifndef LIBMESH_HAVE_VTK
  libMesh::err << "Cannot read VTK file: " << name
	        << "\nYou must have VTK installed and correctly configured to read VTK meshes."
	        << std::endl;
  libmesh_error();

#else
//  libMesh::out<<"read "<<name <<std::endl;
  vtkXMLUnstructuredGridReader *reader = vtkXMLUnstructuredGridReader::New();
  reader->SetFileName( name.c_str() );
  //libMesh::out<<"force read"<<std::endl;
  // Force reading
  reader->Update();

  // read in the grid
//  vtkUnstructuredGrid *grid = reader->GetOutput();
  _vtk_grid = reader->GetOutput();
  _vtk_grid->Update();
  reader->Delete();

  // Get a reference to the mesh
  MeshBase& mesh = MeshInput<MeshBase>::mesh();

  // Clear out any pre-existing data from the Mesh
  mesh.clear();

  // always numbered nicely??, so we can loop like this
  // I'm pretty sure it is numbered nicely
  for (unsigned int i=0; i < static_cast<unsigned int>(_vtk_grid->GetNumberOfPoints()); ++i)
    {
      // add to the id map
      // and add the actual point
      double * pnt = _vtk_grid->GetPoint(static_cast<vtkIdType>(i));
      Point xyz(pnt[0],pnt[1],pnt[2]);
      Node* newnode = mesh.add_point(xyz,i);

      // Add node to the nodes vector &
      // tell the MeshData object the foreign node id.
      if (this->_mesh_data != NULL)
	this->_mesh_data->add_foreign_node_id (newnode, i);
    }

  for (unsigned int i=0; i < static_cast<unsigned int>(_vtk_grid->GetNumberOfCells()); ++i)
    {
      vtkCell* cell = _vtk_grid->GetCell(i);
      Elem* elem = NULL;  // Initialize to avoid compiler warning
      switch(cell->GetCellType())
	{
        // FIXME - we're not supporting 2D VTK input yet!? [RHS]
	case VTK_TETRA:
	  elem = new Tet4();
	  break;
	case VTK_WEDGE:
	  elem = new Prism6();
	  break;
	case VTK_HEXAHEDRON:
	  elem = new Hex8();
	  break;
	case VTK_PYRAMID:
	  elem = new Pyramid5();
	  break;
	case VTK_QUADRATIC_HEXAHEDRON:
  	  elem = new Hex20();
	  break;
	case VTK_QUADRATIC_TETRA:
	  elem = new Tet10();
	  break;
	default:
	  libMesh::err << "element type not implemented in vtkinterface " << cell->GetCellType() << std::endl;
	  libmesh_error();
     break;
	}

  // get the straightforward numbering from the VTK cells
  for(unsigned int j=0;j<elem->n_nodes();++j){
	  elem->set_node(j) = mesh.node_ptr(cell->GetPointId(j));
  }
  // then get the connectivity
  std::vector<unsigned int> conn;
  elem->connectivity(0,VTK,conn);
  // then reshuffle the nodes according to the connectivity, this
  // two-time-assign would evade the definition of the vtk_mapping
  for(unsigned int j=0;j<conn.size();++j){
	  elem->set_node(j) = mesh.node_ptr(conn[j]);
  }
  elem->set_id(i);

  elems_of_dimension[elem->dim()] = true;

  mesh.add_elem(elem);
  } // end loop over VTK cells
  _vtk_grid->Delete();

  // Set the mesh dimension to the largest encountered for an element
  for (unsigned int i=0; i!=4; ++i)
    if (elems_of_dimension[i])
      mesh.set_mesh_dimension(i);

#if LIBMESH_DIM < 3
  if (mesh.mesh_dimension() > LIBMESH_DIM)
    {
      libMesh::err << "Cannot open dimension " <<
		      mesh.mesh_dimension() <<
		      " mesh file when configured without " <<
                      mesh.mesh_dimension() << "D support." <<
                      std::endl;
      libmesh_error();
    }
#endif

#endif // LIBMESH_HAVE_VTK
}


void VTKIO::write_nodal_data (const std::string& fname,
                              const std::vector<Number>& soln,
                              const std::vector<std::string>& names)
{
#ifndef LIBMESH_HAVE_VTK

  libMesh::err << "Cannot write VTK file: " << fname
	        << "\nYou must have VTK installed and correctly configured to read VTK meshes."
	        << std::endl;
  libmesh_error();

#else

  const MeshBase & mesh = MeshOutput<MeshBase>::mesh();

  // check if the filename extension is pvtu
  libmesh_assert(fname.substr(fname.rfind("."),fname.size())==".pvtu");
 
  // we only use Unstructured grids
  _vtk_grid = vtkUnstructuredGrid::New();
  vtkSmartPointer<vtkXMLPUnstructuredGridWriter> writer = vtkXMLPUnstructuredGridWriter::New();

  // add nodes to the grid and update _local_node_map
  _local_node_map.clear();
  this->nodes_to_vtk();

  // add cells to the grid
  this->cells_to_vtk();

  // add nodal solutions to the grid, if solutions are given
  if (names.size() > 0)
    {
      unsigned int num_vars = names.size();
      unsigned int num_nodes = mesh.n_nodes();

      for (unsigned int variable = 0; variable < num_vars; ++variable)
        {
          vtkSmartPointer<vtkDoubleArray> data = vtkDoubleArray::New();
          data->SetName(names[variable].c_str());

          // number of local and ghost nodes
          data->SetNumberOfValues(_local_node_map.size());

          // loop over all nodes and get the solution for the current
          // variable, if the node is in the current partition
          for (unsigned int k = 0; k < num_nodes; ++k)
            {
              if (_local_node_map.find(k) == _local_node_map.end())
                continue; // not a local node
              data->SetValue(_local_node_map[k], soln[k*num_vars + variable]);
            }
          _vtk_grid->GetPointData()->AddArray(data);
          data->Delete();
        }
    }

  // Tell the writer how many partitions exist and on which processor
  // we are currently
  writer->SetNumberOfPieces(libMesh::n_processors());
  writer->SetStartPiece(libMesh::processor_id());
  writer->SetEndPiece(libMesh::processor_id());
 
  // partitions overlap by one node
  // FIXME: According to this document
  // http://paraview.org/Wiki/images/5/51/SC07_tut107_ParaView_Handouts.pdf
  // the ghosts are cells rather than nodes.
  writer->SetGhostLevel(1);

  writer->SetInput(_vtk_grid);
  writer->SetFileName(fname.c_str());
  writer->SetDataModeToAscii();

  // compress the output, if desired (switches also to binary)
  if (this->_compress)
    {
#if !VTK_VERSION_LESS_THAN(5,6,0)
      writer->SetCompressorTypeToZLib();
#else
      libmesh_do_once(libMesh::out << "Compression not implemented with old VTK libs!" << std::endl;);
#endif
    }

  writer->Write();

  _vtk_grid->Delete();
  writer->Delete();
#endif
}

/**
 * Output the mesh without solutions to a .pvtu file
 */
void VTKIO::write (const std::string& name) {
  std::vector<Number> soln;
  std::vector<std::string> names;
  this->write_nodal_data(name, soln, names);
}

} // namespace libMesh

//  vim: sw=3 ts=3
