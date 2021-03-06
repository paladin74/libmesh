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

// Local includes
#include "libmesh/libmesh_config.h"

#ifdef LIBMESH_ENABLE_INFINITE_ELEMENTS

// C++ includes

// Local includes cont'd
#include "libmesh/cell_inf_prism12.h"
#include "libmesh/edge_edge3.h"
#include "libmesh/edge_inf_edge2.h"
#include "libmesh/face_tri6.h"
#include "libmesh/face_inf_quad6.h"
#include "libmesh/side.h"

namespace libMesh
{


// ------------------------------------------------------------
// InfPrism12 class static member initializations
const unsigned int InfPrism12::side_nodes_map[4][6] =
{
  { 0, 1, 2, 6, 7, 8},  // Side 0
  { 0, 1, 3, 4, 6, 9},  // Side 1
  { 1, 2, 4, 5, 7, 10}, // Side 2
  { 2, 0, 5, 3, 8, 11}  // Side 3
};

const unsigned int InfPrism12::edge_nodes_map[6][3] =
{
  { 0, 1, 6},  // Side 0
  { 1, 2, 7},  // Side 1
  { 0, 2, 8},  // Side 2
  { 0, 3, 99}, // Side 3
  { 1, 4, 99}, // Side 4
  { 2, 5, 99}  // Side 5
};


// ------------------------------------------------------------
// InfPrism12 class member functions

bool InfPrism12::is_vertex(const unsigned int i) const
{
  if (i < 3)
    return true;
  return false;
}

bool InfPrism12::is_edge(const unsigned int i) const
{
  if (i < 3)
    return false;
  if (i > 8)
    return false;
  return true;
}

bool InfPrism12::is_face(const unsigned int i) const
{
  if (i > 8)
    return true;
  return false;
}

bool InfPrism12::is_node_on_side(const unsigned int n,
				 const unsigned int s) const
{
  libmesh_assert_less (s, n_sides());
  for (unsigned int i = 0; i != 6; ++i)
    if (side_nodes_map[s][i] == n)
      return true;
  return false;
}

bool InfPrism12::is_node_on_edge(const unsigned int n,
				 const unsigned int e) const
{
  libmesh_assert_less (e, n_edges());
  for (unsigned int i = 0; i != 3; ++i)
    if (edge_nodes_map[e][i] == n)
      return true;
  return false;
}

AutoPtr<Elem> InfPrism12::build_side (const unsigned int i,
				      bool proxy) const
{
  libmesh_assert_less (i, this->n_sides());

  if (proxy)
    {
      switch (i)
	{
	  // base
	case 0:
	  {
	    AutoPtr<Elem> ap(new Side<Tri6,InfPrism12>(this,i));
	    return ap;
	  }
	  // ifem sides
	case 1:
	case 2:
	case 3:
	  {
	    AutoPtr<Elem> ap(new Side<InfQuad6,InfPrism12>(this,i));
	    return ap;
	  }
	default:
	  libmesh_error();
	}
    }

  else
    {
      switch (i)
	{
	case 0:  // the triangular face at z=-1, base face
	  {
	    AutoPtr<Elem> face(new Tri6);

	    // Note that for this face element, the normal points inward
	    face->set_node(0) = this->get_node(0);
	    face->set_node(1) = this->get_node(1);
	    face->set_node(2) = this->get_node(2);
	    face->set_node(3) = this->get_node(6);
	    face->set_node(4) = this->get_node(7);
	    face->set_node(5) = this->get_node(8);

	    return face;
	  }

	case 1:  // the quad face at y=0
	  {
	    AutoPtr<Elem> face(new InfQuad6);

	    face->set_node(0) = this->get_node(0);
	    face->set_node(1) = this->get_node(1);
	    face->set_node(2) = this->get_node(3);
	    face->set_node(3) = this->get_node(4);
	    face->set_node(4) = this->get_node(6);
	    face->set_node(5) = this->get_node(9);

	    return face;
	  }

	case 2:  // the other quad face
	  {
	    AutoPtr<Elem> face(new InfQuad6);

	    face->set_node(0) = this->get_node(1);
	    face->set_node(1) = this->get_node(2);
	    face->set_node(2) = this->get_node(4);
	    face->set_node(3) = this->get_node(5);
	    face->set_node(4) = this->get_node(7);
	    face->set_node(5) = this->get_node(10);

	    return face;
	  }

	case 3: // the quad face at x=0
	  {
	    AutoPtr<Elem> face(new InfQuad6);

	    face->set_node(0) = this->get_node(2);
	    face->set_node(1) = this->get_node(0);
	    face->set_node(2) = this->get_node(5);
	    face->set_node(3) = this->get_node(3);
	    face->set_node(4) = this->get_node(8);
	    face->set_node(5) = this->get_node(11);

	    return face;
	  }

	default:
	  {
	    libmesh_error();
	    AutoPtr<Elem> ap(NULL);  return ap;
	  }
	}
    }


  // We'll never get here.
  libmesh_error();
  AutoPtr<Elem> ap(NULL);  return ap;
}


AutoPtr<Elem> InfPrism12::build_edge (const unsigned int i) const
{
  libmesh_assert_less (i, this->n_edges());

  if (i < 3) // base edges
    return AutoPtr<Elem>(new SideEdge<Edge3,InfPrism12>(this,i));
  // infinite edges
  return AutoPtr<Elem>(new SideEdge<InfEdge2,InfPrism12>(this,i));
}


void InfPrism12::connectivity(const unsigned int sc,
			      const IOPackage iop,
			      std::vector<unsigned int>& conn) const
{
  libmesh_assert(_nodes);
  libmesh_assert_less (sc, this->n_sub_elem());
  libmesh_assert_not_equal_to (iop, INVALID_IO_PACKAGE);

  switch (iop)
    {
    case TECPLOT:
      {
	conn.resize(8);
	switch (sc)
	  {
	  case 0:

	    // guess this is a collapsed hex8
	    conn[0] = this->node(0)+1;
	    conn[1] = this->node(6)+1;
	    conn[2] = this->node(8)+1;
	    conn[3] = this->node(8)+1;
	    conn[4] = this->node(3)+1;
	    conn[5] = this->node(9)+1;
	    conn[6] = this->node(11)+1;
	    conn[7] = this->node(11)+1;

	    return;

	  case 1:

	    conn[0] = this->node(6)+1;
	    conn[1] = this->node(7)+1;
	    conn[2] = this->node(8)+1;
	    conn[3] = this->node(8)+1;
	    conn[4] = this->node(9)+1;
	    conn[5] = this->node(10)+1;
	    conn[6] = this->node(11)+1;
	    conn[7] = this->node(11)+1;

	    return;

	  case 2:

	    conn[0] = this->node(6)+1;
	    conn[1] = this->node(1)+1;
	    conn[2] = this->node(7)+1;
	    conn[3] = this->node(7)+1;
	    conn[4] = this->node(9)+1;
	    conn[5] = this->node(4)+1;
	    conn[6] = this->node(10)+1;
	    conn[7] = this->node(10)+1;

	    return;

	  case 3:

	    conn[0] = this->node(8)+1;
	    conn[1] = this->node(7)+1;
	    conn[2] = this->node(2)+1;
	    conn[3] = this->node(2)+1;
	    conn[4] = this->node(11)+1;
	    conn[5] = this->node(10)+1;
	    conn[6] = this->node(5)+1;
	    conn[7] = this->node(5)+1;

	    return;

	  default:
	    libmesh_error();

	  }

      }

    default:
      libmesh_error();
    }

  libmesh_error();
}





unsigned short int InfPrism12::second_order_adjacent_vertex (const unsigned int n,
							     const unsigned int v) const
{
  libmesh_assert_greater_equal (n, this->n_vertices());
  libmesh_assert_less (n, this->n_nodes());
  libmesh_assert_less (v, 2);
  return _second_order_adjacent_vertices[n-this->n_vertices()][v];
}



const unsigned short int InfPrism12::_second_order_adjacent_vertices[6][2] =
{
  { 0,  1}, // vertices adjacent to node 6
  { 1,  2}, // vertices adjacent to node 7
  { 0,  2}, // vertices adjacent to node 8

  { 3,  4}, // vertices adjacent to node 9
  { 4,  5}, // vertices adjacent to node 10
  { 3,  5}  // vertices adjacent to node 11
};



std::pair<unsigned short int, unsigned short int>
InfPrism12::second_order_child_vertex (const unsigned int n) const
{
  libmesh_assert_greater_equal (n, this->n_vertices());
  libmesh_assert_less (n, this->n_nodes());

  return std::pair<unsigned short int, unsigned short int>
    (_second_order_vertex_child_number[n],
     _second_order_vertex_child_index[n]);
}



const unsigned short int InfPrism12::_second_order_vertex_child_number[12] =
{
  99,99,99,99,99,99, // Vertices
  0,1,0,             // Edges
  0,1,0              // Faces
};



const unsigned short int InfPrism12::_second_order_vertex_child_index[12] =
{
  99,99,99,99,99,99, // Vertices
  1,2,2,             // Edges
  4,5,5              // Faces
};



#ifdef LIBMESH_ENABLE_AMR

const float InfPrism12::_embedding_matrix[4][12][12] =
{
  // embedding matrix for child 0
  {
    //          0           1           2           3           4           5           6           7           8           9          10          11 th parent Node
    {         1.0,        0.0,        0.0,        0.0,        0.0,        0.0,        0.0,        0.0,        0.0,        0.0,        0.0,        0.0}, // 0th child N.
    {         0.0,        0.0,        0.0,        0.0,        0.0,        0.0,        1.0,        0.0,        0.0,        0.0,        0.0,        0.0}, // 1
    {         0.0,        0.0,        0.0,        0.0,        0.0,        0.0,        0.0,        0.0,        1.0,        0.0,        0.0,        0.0}, // 2
    {         0.0,        0.0,        0.0,        1.0,        0.0,        0.0,        0.0,        0.0,        0.0,        0.0,        0.0,        0.0}, // 3
    {         0.0,        0.0,        0.0,        0.0,        0.0,        0.0,        0.0,        0.0,        0.0,        1.0,        0.0,        0.0}, // 4
    {         0.0,        0.0,        0.0,        0.0,        0.0,        0.0,        0.0,        0.0,        0.0,        0.0,        0.0,        1.0}, // 5
    {       0.375,     -0.125,        0.0,        0.0,        0.0,        0.0,       0.75,        0.0,        0.0,        0.0,        0.0,        0.0}, // 6
    {         0.0,     -0.125,     -0.125,        0.0,        0.0,        0.0,        0.5,       0.25,        0.5,        0.0,        0.0,        0.0}, // 7
    {       0.375,        0.0,     -0.125,        0.0,        0.0,        0.0,        0.0,        0.0,       0.75,        0.0,        0.0,        0.0}, // 8
    {         0.0,        0.0,        0.0,      0.375,     -0.125,        0.0,        0.0,        0.0,        0.0,       0.75,        0.0,        0.0}, // 9
    {         0.0,        0.0,        0.0,        0.0,     -0.125,     -0.125,        0.0,        0.0,        0.0,        0.5,       0.25,        0.5}, // 10
    {         0.0,        0.0,        0.0,      0.375,        0.0,     -0.125,        0.0,        0.0,        0.0,        0.0,        0.0,       0.75}  // 11
  },

  // embedding matrix for child 1
  {
    //          0           1           2           3           4           5           6           7           8           9          10          11 th parent Node
    {         0.0,        0.0,        0.0,        0.0,        0.0,        0.0,        1.0,        0.0,        0.0,        0.0,        0.0,        0.0}, // 0th child N.
    {         0.0,        1.0,        0.0,        0.0,        0.0,        0.0,        0.0,        0.0,        0.0,        0.0,        0.0,        0.0}, // 1
    {         0.0,        0.0,        0.0,        0.0,        0.0,        0.0,        0.0,        1.0,        0.0,        0.0,        0.0,        0.0}, // 2
    {         0.0,        0.0,        0.0,        0.0,        0.0,        0.0,        0.0,        0.0,        0.0,        1.0,        0.0,        0.0}, // 3
    {         0.0,        0.0,        0.0,        0.0,        1.0,        0.0,        0.0,        0.0,        0.0,        0.0,        0.0,        0.0}, // 4
    {         0.0,        0.0,        0.0,        0.0,        0.0,        0.0,        0.0,        0.0,        0.0,        0.0,        1.0,        0.0}, // 5
    {      -0.125,      0.375,        0.0,        0.0,        0.0,        0.0,       0.75,        0.0,        0.0,        0.0,        0.0,        0.0}, // 6
    {         0.0,      0.375,     -0.125,        0.0,        0.0,        0.0,        0.0,       0.75,        0.0,        0.0,        0.0,        0.0}, // 7
    {      -0.125,        0.0,     -0.125,        0.0,        0.0,        0.0,        0.5,        0.5,       0.25,        0.0,        0.0,        0.0}, // 8
    {         0.0,        0.0,        0.0,     -0.125,      0.375,        0.0,        0.0,        0.0,        0.0,       0.75,        0.0,        0.0}, // 9
    {         0.0,        0.0,        0.0,        0.0,      0.375,     -0.125,        0.0,        0.0,        0.0,        0.0,       0.75,        0.0}, // 10
    {         0.0,        0.0,        0.0,     -0.125,        0.0,     -0.125,        0.0,        0.0,        0.0,        0.5,        0.5,       0.25}  // 11
  },

  // embedding matrix for child 2
  {
    //          0           1           2           3           4           5           6           7           8           9          10          11 th parent Node
    {         0.0,        0.0,        0.0,        0.0,        0.0,        0.0,        0.0,        0.0,        1.0,        0.0,        0.0,        0.0}, // 0th child N.
    {         0.0,        0.0,        0.0,        0.0,        0.0,        0.0,        0.0,        1.0,        0.0,        0.0,        0.0,        0.0}, // 1
    {         0.0,        0.0,        1.0,        0.0,        0.0,        0.0,        0.0,        0.0,        0.0,        0.0,        0.0,        0.0}, // 2
    {         0.0,        0.0,        0.0,        0.0,        0.0,        0.0,        0.0,        0.0,        0.0,        0.0,        0.0,        1.0}, // 3
    {         0.0,        0.0,        0.0,        0.0,        0.0,        0.0,        0.0,        0.0,        0.0,        0.0,        1.0,        0.0}, // 4
    {         0.0,        0.0,        0.0,        0.0,        0.0,        1.0,        0.0,        0.0,        0.0,        0.0,        0.0,        0.0}, // 5
    {      -0.125,     -0.125,        0.0,        0.0,        0.0,        0.0,       0.25,        0.5,        0.5,        0.0,        0.0,        0.0}, // 6
    {         0.0,     -0.125,      0.375,        0.0,        0.0,        0.0,        0.0,       0.75,        0.0,        0.0,        0.0,        0.0}, // 7
    {      -0.125,        0.0,      0.375,        0.0,        0.0,        0.0,        0.0,        0.0,       0.75,        0.0,        0.0,        0.0}, // 8
    {         0.0,        0.0,        0.0,     -0.125,     -0.125,        0.0,        0.0,        0.0,        0.0,       0.25,        0.5,        0.5}, // 9
    {         0.0,        0.0,        0.0,        0.0,     -0.125,      0.375,        0.0,        0.0,        0.0,        0.0,       0.75,        0.0}, // 10
    {         0.0,        0.0,        0.0,     -0.125,        0.0,      0.375,        0.0,        0.0,        0.0,        0.0,        0.0,       0.75}  // 11
  },

  // embedding matrix for child 3
  {
    //          0           1           2           3           4           5           6           7           8           9          10          11 th parent Node
    {         0.0,        0.0,        0.0,        0.0,        0.0,        0.0,        1.0,        0.0,        0.0,        0.0,        0.0,        0.0}, // 0th child N.
    {         0.0,        0.0,        0.0,        0.0,        0.0,        0.0,        0.0,        1.0,        0.0,        0.0,        0.0,        0.0}, // 1
    {         0.0,        0.0,        0.0,        0.0,        0.0,        0.0,        0.0,        0.0,        1.0,        0.0,        0.0,        0.0}, // 2
    {         0.0,        0.0,        0.0,        0.0,        0.0,        0.0,        0.0,        0.0,        0.0,        1.0,        0.0,        0.0}, // 3
    {         0.0,        0.0,        0.0,        0.0,        0.0,        0.0,        0.0,        0.0,        0.0,        0.0,        1.0,        0.0}, // 4
    {         0.0,        0.0,        0.0,        0.0,        0.0,        0.0,        0.0,        0.0,        0.0,        0.0,        0.0,        1.0}, // 5
    {      -0.125,        0.0,     -0.125,        0.0,        0.0,        0.0,        0.5,        0.5,       0.25,        0.0,        0.0,        0.0}, // 6
    {      -0.125,     -0.125,        0.0,        0.0,        0.0,        0.0,       0.25,        0.5,        0.5,        0.0,        0.0,        0.0}, // 7
    {         0.0,     -0.125,     -0.125,        0.0,        0.0,        0.0,        0.5,       0.25,        0.5,        0.0,        0.0,        0.0}, // 8
    {         0.0,        0.0,        0.0,     -0.125,        0.0,     -0.125,        0.0,        0.0,        0.0,        0.5,        0.5,       0.25}, // 9
    {         0.0,        0.0,        0.0,     -0.125,     -0.125,        0.0,        0.0,        0.0,        0.0,       0.25,        0.5,        0.5}, // 10
    {         0.0,        0.0,        0.0,        0.0,     -0.125,     -0.125,        0.0,        0.0,        0.0,        0.5,       0.25,        0.5}  // 11
  }

};




#endif

} // namespace libMesh

#endif // ifdef LIBMESH_ENABLE_INFINITE_ELEMENTS
