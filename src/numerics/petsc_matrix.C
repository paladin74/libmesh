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
#include <unistd.h> // mkstemp

#include "libmesh/libmesh_config.h"

#ifdef LIBMESH_HAVE_PETSC

// Local includes
#include "libmesh/petsc_matrix.h"
#include "libmesh/dof_map.h"
#include "libmesh/dense_matrix.h"
#include "libmesh/petsc_vector.h"

namespace libMesh
{


//-----------------------------------------------------------------------
// PetscMatrix members


// Constructor 
template <typename T>
PetscMatrix<T>::PetscMatrix()
  : _destroy_mat_on_exit(true)
{}



// Constructor taking an existing Mat but not the responsibility
// for destroying it
template <typename T>
PetscMatrix<T>::PetscMatrix(Mat m)
  : _destroy_mat_on_exit(false)
{
  this->_mat = m;
  this->_is_initialized = true;
}



// Destructor
template <typename T>
PetscMatrix<T>::~PetscMatrix()
{
  this->clear();
}


template <typename T>
void PetscMatrix<T>::init (const numeric_index_type m,
			   const numeric_index_type n,
			   const numeric_index_type m_l,
			   const numeric_index_type n_l,
			   const numeric_index_type nnz,
			   const numeric_index_type noz)
{
  // We allow 0x0 matrices now
  //if ((m==0) || (n==0))
  //  return;

  // Clear initialized matrices
  if (this->initialized())
    this->clear();

  this->_is_initialized = true;


  PetscErrorCode ierr     = 0;
  PetscInt m_global = static_cast<PetscInt>(m);
  PetscInt n_global = static_cast<PetscInt>(n);
  PetscInt m_local  = static_cast<PetscInt>(m_l);
  PetscInt n_local  = static_cast<PetscInt>(n_l);
  PetscInt n_nz     = static_cast<PetscInt>(nnz);
  PetscInt n_oz     = static_cast<PetscInt>(noz);

  ierr = MatCreate(libMesh::COMM_WORLD, &_mat);
  CHKERRABORT(libMesh::COMM_WORLD,ierr);
  ierr = MatSetSizes(_mat, m_local, n_local, m_global, n_global);
  CHKERRABORT(libMesh::COMM_WORLD,ierr);
  ierr = MatSetType(_mat, MATAIJ); // Automatically chooses seqaij or mpiaij
  CHKERRABORT(libMesh::COMM_WORLD,ierr);

  // Make it an error for PETSc to allocate new nonzero entries during assembly
#if PETSC_VERSION_LESS_THAN(3,0,0)
  ierr = MatSetOption(_mat, MAT_NEW_NONZERO_ALLOCATION_ERR);
#else
  ierr = MatSetOption(_mat, MAT_NEW_NONZERO_ALLOCATION_ERR, PETSC_TRUE);
#endif
  CHKERRABORT(libMesh::COMM_WORLD,ierr);

  // Is prefix information available somewhere? Perhaps pass in the system name?
  ierr = MatSetOptionsPrefix(_mat, "");
  CHKERRABORT(libMesh::COMM_WORLD,ierr);
  ierr = MatSetFromOptions(_mat);
  CHKERRABORT(libMesh::COMM_WORLD,ierr);
  ierr = MatSeqAIJSetPreallocation(_mat, n_nz, PETSC_NULL);
  CHKERRABORT(libMesh::COMM_WORLD,ierr);
  ierr = MatMPIAIJSetPreallocation(_mat, n_nz, PETSC_NULL, n_oz, PETSC_NULL);
  CHKERRABORT(libMesh::COMM_WORLD,ierr);

  this->zero ();
}


template <typename T>
void PetscMatrix<T>::init (const numeric_index_type m,
			   const numeric_index_type n,
			   const numeric_index_type m_l,
			   const numeric_index_type n_l,
			   const std::vector<numeric_index_type>& n_nz,
			   const std::vector<numeric_index_type>& n_oz)
{
  // Clear initialized matrices
  if (this->initialized())
    this->clear();

  this->_is_initialized = true;

  // Make sure the sparsity pattern isn't empty unless the matrix is 0x0
  libmesh_assert_equal_to (n_nz.size(), m_l);
  libmesh_assert_equal_to (n_oz.size(), m_l);

  PetscErrorCode ierr     = 0;
  PetscInt m_global = static_cast<PetscInt>(m);
  PetscInt n_global = static_cast<PetscInt>(n);
  PetscInt m_local  = static_cast<PetscInt>(m_l);
  PetscInt n_local  = static_cast<PetscInt>(n_l);

  ierr = MatCreate(libMesh::COMM_WORLD, &_mat);
  CHKERRABORT(libMesh::COMM_WORLD,ierr);
  ierr = MatSetSizes(_mat, m_local, n_local, m_global, n_global);
  CHKERRABORT(libMesh::COMM_WORLD,ierr);
  ierr = MatSetType(_mat, MATAIJ); // Automatically chooses seqaij or mpiaij
  CHKERRABORT(libMesh::COMM_WORLD,ierr);
  // Is prefix information available somewhere? Perhaps pass in the system name?
  ierr = MatSetOptionsPrefix(_mat, "");
  CHKERRABORT(libMesh::COMM_WORLD,ierr);
  ierr = MatSetFromOptions(_mat);
  CHKERRABORT(libMesh::COMM_WORLD,ierr);

  ierr = MatSeqAIJSetPreallocation(_mat, 0, (PetscInt*)(n_nz.empty()?NULL:&n_nz[0]));
  CHKERRABORT(libMesh::COMM_WORLD,ierr);
  ierr = MatMPIAIJSetPreallocation(_mat, 0, (PetscInt*)(n_nz.empty()?NULL:&n_nz[0]),
                                         0, (PetscInt*)(n_oz.empty()?NULL:&n_oz[0]));
  CHKERRABORT(libMesh::COMM_WORLD,ierr);

  this->zero();
}


template <typename T>
void PetscMatrix<T>::init ()
{
  libmesh_assert(this->_dof_map);

  // Clear initialized matrices
  if (this->initialized())
    this->clear();

  this->_is_initialized = true;


  const numeric_index_type m   = this->_dof_map->n_dofs();
  const numeric_index_type n   = m;
  const numeric_index_type n_l = this->_dof_map->n_dofs_on_processor(libMesh::processor_id());
  const numeric_index_type m_l = n_l;


  const std::vector<numeric_index_type>& n_nz = this->_dof_map->get_n_nz();
  const std::vector<numeric_index_type>& n_oz = this->_dof_map->get_n_oz();

  // Make sure the sparsity pattern isn't empty unless the matrix is 0x0
  libmesh_assert_equal_to (n_nz.size(), m_l);
  libmesh_assert_equal_to (n_oz.size(), m_l);

  // We allow 0x0 matrices now
  //if (m==0)
  //  return;

  PetscErrorCode ierr     = 0;
  PetscInt m_global = static_cast<PetscInt>(m);
  PetscInt n_global = static_cast<PetscInt>(n);
  PetscInt m_local  = static_cast<PetscInt>(m_l);
  PetscInt n_local  = static_cast<PetscInt>(n_l);

  ierr = MatCreate(libMesh::COMM_WORLD, &_mat);
  CHKERRABORT(libMesh::COMM_WORLD,ierr);
  ierr = MatSetSizes(_mat, m_local, n_local, m_global, n_global);
  CHKERRABORT(libMesh::COMM_WORLD,ierr);
  ierr = MatSetType(_mat, MATAIJ); // Automatically chooses seqaij or mpiaij
  CHKERRABORT(libMesh::COMM_WORLD,ierr);
  // Is prefix information available somewhere? Perhaps pass in the system name?
  ierr = MatSetOptionsPrefix(_mat, "");
  CHKERRABORT(libMesh::COMM_WORLD,ierr);
  ierr = MatSetFromOptions(_mat);
  CHKERRABORT(libMesh::COMM_WORLD,ierr);
  
  ierr = MatSeqAIJSetPreallocation(_mat, 0, (PetscInt*)(n_nz.empty()?NULL:&n_nz[0]));
  CHKERRABORT(libMesh::COMM_WORLD,ierr);
  ierr = MatMPIAIJSetPreallocation(_mat, 0, (PetscInt*)(n_nz.empty()?NULL:&n_nz[0]),
                                         0, (PetscInt*)(n_oz.empty()?NULL:&n_oz[0]));
  CHKERRABORT(libMesh::COMM_WORLD,ierr);

  this->zero();
}



template <typename T>
void PetscMatrix<T>::zero ()
{
  libmesh_assert (this->initialized());

  semiparallel_only();

  PetscErrorCode ierr=0;

  PetscInt m_l, n_l;

  ierr = MatGetLocalSize(_mat,&m_l,&n_l);
  CHKERRABORT(libMesh::COMM_WORLD,ierr);

  if (n_l)
    {
      ierr = MatZeroEntries(_mat);
      CHKERRABORT(libMesh::COMM_WORLD,ierr);
    }
}

template <typename T>
void PetscMatrix<T>::zero_rows (std::vector<numeric_index_type> & rows, T diag_value)
{
  libmesh_assert (this->initialized());

  semiparallel_only();

  PetscErrorCode ierr=0;

#if PETSC_VERSION_RELEASE && PETSC_VERSION_LESS_THAN(3,1,1)
  if(!rows.empty())
    ierr = MatZeroRows(_mat, rows.size(), (PetscInt*)&rows[0], diag_value);
  else
    ierr = MatZeroRows(_mat, 0, PETSC_NULL, diag_value);
#else
  // As of petsc-dev at the time of 3.1.0, MatZeroRows now takes two additional
  // optional arguments.  The optional arguments (x,b) can be used to specify the
  // solutions for the zeroed rows (x) and right hand side (b) to update.
  // Could be useful for setting boundary conditions...
  if(!rows.empty())
    ierr = MatZeroRows(_mat, rows.size(), (PetscInt*)&rows[0], diag_value, PETSC_NULL, PETSC_NULL);
  else
    ierr = MatZeroRows(_mat, 0, PETSC_NULL, diag_value, PETSC_NULL, PETSC_NULL);
#endif

  CHKERRABORT(libMesh::COMM_WORLD,ierr);
}

template <typename T>
void PetscMatrix<T>::clear ()
{
  PetscErrorCode ierr=0;

  if ((this->initialized()) && (this->_destroy_mat_on_exit))
    {
      semiparallel_only();

      ierr = LibMeshMatDestroy (&_mat);
             CHKERRABORT(libMesh::COMM_WORLD,ierr);

      this->_is_initialized = false;
    }
}



template <typename T>
Real PetscMatrix<T>::l1_norm () const
{
  libmesh_assert (this->initialized());

  semiparallel_only();

  PetscErrorCode ierr=0;
  PetscReal petsc_value;
  Real value;

  libmesh_assert (this->closed());

  ierr = MatNorm(_mat, NORM_1, &petsc_value);
         CHKERRABORT(libMesh::COMM_WORLD,ierr);

  value = static_cast<Real>(petsc_value);

  return value;
}



template <typename T>
Real PetscMatrix<T>::linfty_norm () const
{
  libmesh_assert (this->initialized());

  semiparallel_only();

  PetscErrorCode ierr=0;
  PetscReal petsc_value;
  Real value;

  libmesh_assert (this->closed());

  ierr = MatNorm(_mat, NORM_INFINITY, &petsc_value);
         CHKERRABORT(libMesh::COMM_WORLD,ierr);

  value = static_cast<Real>(petsc_value);

  return value;
}



template <typename T>
void PetscMatrix<T>::print_matlab (const std::string name) const
{
  libmesh_assert (this->initialized());

  semiparallel_only();

  // libmesh_assert (this->closed());
  this->close();

  PetscErrorCode ierr=0;
  PetscViewer petsc_viewer;


  ierr = PetscViewerCreate (libMesh::COMM_WORLD,
			    &petsc_viewer);
         CHKERRABORT(libMesh::COMM_WORLD,ierr);

  /**
   * Create an ASCII file containing the matrix
   * if a filename was provided.
   */
  if (name != "NULL")
    {
      ierr = PetscViewerASCIIOpen( libMesh::COMM_WORLD,
				   name.c_str(),
				   &petsc_viewer);
             CHKERRABORT(libMesh::COMM_WORLD,ierr);

      ierr = PetscViewerSetFormat (petsc_viewer,
				   PETSC_VIEWER_ASCII_MATLAB);
             CHKERRABORT(libMesh::COMM_WORLD,ierr);

      ierr = MatView (_mat, petsc_viewer);
             CHKERRABORT(libMesh::COMM_WORLD,ierr);
    }

  /**
   * Otherwise the matrix will be dumped to the screen.
   */
  else
    {
      ierr = PetscViewerSetFormat (PETSC_VIEWER_STDOUT_WORLD,
				   PETSC_VIEWER_ASCII_MATLAB);
             CHKERRABORT(libMesh::COMM_WORLD,ierr);

      ierr = MatView (_mat, PETSC_VIEWER_STDOUT_WORLD);
             CHKERRABORT(libMesh::COMM_WORLD,ierr);
    }


  /**
   * Destroy the viewer.
   */
  ierr = LibMeshPetscViewerDestroy (&petsc_viewer);
         CHKERRABORT(libMesh::COMM_WORLD,ierr);
}





template <typename T>
void PetscMatrix<T>::print_personal(std::ostream& os) const
{
  libmesh_assert (this->initialized());

  // Routine must be called in parallel on parallel matrices
  // and serial on serial matrices.
  semiparallel_only();

// #ifndef NDEBUG
//   if (os != std::cout)
//     libMesh::err << "Warning! PETSc can only print to std::cout!" << std::endl;
// #endif

  // Matrix must be in an assembled state to be printed
  this->close();

  PetscErrorCode ierr=0;

  // Print to screen if ostream is stdout
  if (os == std::cout)
    {
      ierr = MatView(_mat, PETSC_VIEWER_STDOUT_SELF);
      CHKERRABORT(libMesh::COMM_WORLD,ierr);
    }

  // Otherwise, print to the requested file, in a roundabout way...
  else
    {
      // We will create a temporary filename, and file, for PETSc to
      // write to.
      std::string temp_filename;

      {
	// Template for temporary filename
	char c[] = "temp_petsc_matrix.XXXXXX";

	// Generate temporary, unique filename only on processor 0.  We will
	// use this filename for PetscViewerASCIIOpen, before copying it into
	// the user's stream
	if (libMesh::processor_id() == 0)
	  {
	    int fd = mkstemp(c);

	    // Check to see that mkstemp did not fail.
	    if (fd == -1)
	      libmesh_error();

	    // mkstemp returns a file descriptor for an open file,
	    // so let's close it before we hand it to PETSc!
	    ::close (fd);
	  }

	// Store temporary filename as string, makes it easier to broadcast
	temp_filename = c;
      }

      // Now broadcast the filename from processor 0 to all processors.
      CommWorld.broadcast(temp_filename);

      // PetscViewer object for passing to MatView
      PetscViewer petsc_viewer;

      // This PETSc function only takes a string and handles the opening/closing
      // of the file internally.  Since print_personal() takes a reference to
      // an ostream, we have to do an extra step...  print_personal() should probably
      // have a version that takes a string to get rid of this problem.
      ierr = PetscViewerASCIIOpen( libMesh::COMM_WORLD,
				   temp_filename.c_str(),
				   &petsc_viewer);
      CHKERRABORT(libMesh::COMM_WORLD,ierr);

      // Probably don't need to set the format if it's default...
      //      ierr = PetscViewerSetFormat (petsc_viewer,
      //				   PETSC_VIEWER_DEFAULT);
      //      CHKERRABORT(libMesh::COMM_WORLD,ierr);

      // Finally print the matrix using the viewer
      ierr = MatView (_mat, petsc_viewer);
      CHKERRABORT(libMesh::COMM_WORLD,ierr);

      if (libMesh::processor_id() == 0)
	{
	  // Now the inefficient bit: open temp_filename as an ostream and copy the contents
	  // into the user's desired ostream.  We can't just do a direct file copy, we don't even have the filename!
	  std::ifstream input_stream(temp_filename.c_str());
	  os << input_stream.rdbuf();  // The "most elegant" way to copy one stream into another.
	  // os.close(); // close not defined in ostream

	  // Now remove the temporary file
	  input_stream.close();
	  std::remove(temp_filename.c_str());
	}
    }
}






template <typename T>
void PetscMatrix<T>::add_matrix(const DenseMatrix<T>& dm,
				const std::vector<numeric_index_type>& rows,
				const std::vector<numeric_index_type>& cols)
{
  libmesh_assert (this->initialized());

  const numeric_index_type m = dm.m();
  const numeric_index_type n = dm.n();

  libmesh_assert_equal_to (rows.size(), m);
  libmesh_assert_equal_to (cols.size(), n);

  PetscErrorCode ierr=0;

  // These casts are required for PETSc <= 2.1.5
  ierr = MatSetValues(_mat,
		      m, (PetscInt*) &rows[0],
		      n, (PetscInt*) &cols[0],
		      (PetscScalar*) &dm.get_values()[0],
		      ADD_VALUES);
         CHKERRABORT(libMesh::COMM_WORLD,ierr);
}





template <typename T>
void PetscMatrix<T>::_get_submatrix(SparseMatrix<T>& submatrix,
				    const std::vector<numeric_index_type> &rows,
				    const std::vector<numeric_index_type> &cols,
				    const bool reuse_submatrix) const
{
  // Can only extract submatrices from closed matrices
  this->close();

  // Make sure the SparseMatrix passed in is really a PetscMatrix
  PetscMatrix<T>* petsc_submatrix = libmesh_cast_ptr<PetscMatrix<T>*>(&submatrix);

  // If we're not reusing submatrix and submatrix is already initialized
  // then we need to clear it, otherwise we get a memory leak.
  if( !reuse_submatrix && submatrix.initialized() )
    submatrix.clear();

  // Construct row and column index sets.
  PetscErrorCode ierr=0;
  IS isrow, iscol;

  ierr = ISCreateLibMesh(libMesh::COMM_WORLD,
			 rows.size(),
			 (PetscInt*) &rows[0],
			 PETSC_USE_POINTER,
			 &isrow); CHKERRABORT(libMesh::COMM_WORLD,ierr);

  ierr = ISCreateLibMesh(libMesh::COMM_WORLD,
			 cols.size(),
			 (PetscInt*) &cols[0],
			 PETSC_USE_POINTER,
			 &iscol); CHKERRABORT(libMesh::COMM_WORLD,ierr);

  // Extract submatrix
#if !PETSC_VERSION_LESS_THAN(3,0,1) || !PETSC_VERSION_RELEASE
  ierr = MatGetSubMatrix(_mat,
			 isrow,
			 iscol,
			 (reuse_submatrix ? MAT_REUSE_MATRIX : MAT_INITIAL_MATRIX),
			 &(petsc_submatrix->_mat));  CHKERRABORT(libMesh::COMM_WORLD,ierr);
#else
  ierr = MatGetSubMatrix(_mat,
			 isrow,
			 iscol,
			 PETSC_DECIDE,
			 (reuse_submatrix ? MAT_REUSE_MATRIX : MAT_INITIAL_MATRIX),
			 &(petsc_submatrix->_mat));  CHKERRABORT(libMesh::COMM_WORLD,ierr);
#endif

  // Specify that the new submatrix is initialized and close it.
  petsc_submatrix->_is_initialized = true;
  petsc_submatrix->close();

  // Clean up PETSc data structures
  ierr = LibMeshISDestroy(&isrow); CHKERRABORT(libMesh::COMM_WORLD,ierr);
  ierr = LibMeshISDestroy(&iscol); CHKERRABORT(libMesh::COMM_WORLD,ierr);
}



template <typename T>
void PetscMatrix<T>::get_diagonal (NumericVector<T>& dest) const
{
  // Make sure the NumericVector passed in is really a PetscVector
  PetscVector<T>& petsc_dest = libmesh_cast_ref<PetscVector<T>&>(dest);

  // Call PETSc function.

#if PETSC_VERSION_LESS_THAN(2,3,1)

  libMesh::out << "This method has been developed with PETSc 2.3.1.  "
	        << "No one has made it backwards compatible with older "
	        << "versions of PETSc so far; however, it might work "
	        << "without any change with some older version." << std::endl;
  libmesh_error();

#else

  // Needs a const_cast since PETSc does not work with const.
  PetscErrorCode ierr =
    MatGetDiagonal(const_cast<PetscMatrix<T>*>(this)->mat(),petsc_dest.vec()); CHKERRABORT(libMesh::COMM_WORLD,ierr);

#endif

}



template <typename T>
void PetscMatrix<T>::get_transpose (SparseMatrix<T>& dest) const
{
  // Make sure the SparseMatrix passed in is really a PetscMatrix
  PetscMatrix<T>& petsc_dest = libmesh_cast_ref<PetscMatrix<T>&>(dest);

  // If we aren't reusing the matrix then need to clear dest,
  // otherwise we get a memory leak
  if(&petsc_dest != this)
    dest.clear();

  PetscErrorCode ierr;
#if PETSC_VERSION_LESS_THAN(3,0,0)
  if (&petsc_dest == this)
    ierr = MatTranspose(_mat,PETSC_NULL);
  else
    ierr = MatTranspose(_mat,&petsc_dest._mat);
  CHKERRABORT(libMesh::COMM_WORLD,ierr);
#else
  // FIXME - we can probably use MAT_REUSE_MATRIX in more situations
  if (&petsc_dest == this)
    ierr = MatTranspose(_mat,MAT_REUSE_MATRIX,&petsc_dest._mat);
  else
    ierr = MatTranspose(_mat,MAT_INITIAL_MATRIX,&petsc_dest._mat);
  CHKERRABORT(libMesh::COMM_WORLD,ierr);
#endif

  // Specify that the transposed matrix is initialized and close it.
  petsc_dest._is_initialized = true;
  petsc_dest.close();
}





template <typename T>
void PetscMatrix<T>::close () const
{
  semiparallel_only();

  // BSK - 1/19/2004
  // strictly this check should be OK, but it seems to
  // fail on matrix-free matrices.  Do they falsely
  // state they are assembled?  Check with the developers...
//   if (this->closed())
//     return;

  PetscErrorCode ierr=0;

  ierr = MatAssemblyBegin (_mat, MAT_FINAL_ASSEMBLY);
         CHKERRABORT(libMesh::COMM_WORLD,ierr);
  ierr = MatAssemblyEnd   (_mat, MAT_FINAL_ASSEMBLY);
         CHKERRABORT(libMesh::COMM_WORLD,ierr);
}



template <typename T>
numeric_index_type PetscMatrix<T>::m () const
{
  libmesh_assert (this->initialized());

  PetscInt petsc_m=0, petsc_n=0;
  PetscErrorCode ierr=0;

  ierr = MatGetSize (_mat, &petsc_m, &petsc_n);
         CHKERRABORT(libMesh::COMM_WORLD,ierr);

  return static_cast<numeric_index_type>(petsc_m);
}



template <typename T>
numeric_index_type PetscMatrix<T>::n () const
{
  libmesh_assert (this->initialized());

  PetscInt petsc_m=0, petsc_n=0;
  PetscErrorCode ierr=0;

  ierr = MatGetSize (_mat, &petsc_m, &petsc_n);
         CHKERRABORT(libMesh::COMM_WORLD,ierr);

  return static_cast<numeric_index_type>(petsc_n);
}



template <typename T>
numeric_index_type PetscMatrix<T>::row_start () const
{
  libmesh_assert (this->initialized());

  PetscInt start=0, stop=0;
  PetscErrorCode ierr=0;

  ierr = MatGetOwnershipRange(_mat, &start, &stop);
         CHKERRABORT(libMesh::COMM_WORLD,ierr);

  return static_cast<numeric_index_type>(start);
}



template <typename T>
numeric_index_type PetscMatrix<T>::row_stop () const
{
  libmesh_assert (this->initialized());

  PetscInt start=0, stop=0;
  PetscErrorCode ierr=0;

  ierr = MatGetOwnershipRange(_mat, &start, &stop);
         CHKERRABORT(libMesh::COMM_WORLD,ierr);

  return static_cast<numeric_index_type>(stop);
}



template <typename T>
void PetscMatrix<T>::set (const numeric_index_type i,
			  const numeric_index_type j,
			  const T value)
{
  libmesh_assert (this->initialized());

  PetscErrorCode ierr=0;
  PetscInt i_val=i, j_val=j;

  PetscScalar petsc_value = static_cast<PetscScalar>(value);
  ierr = MatSetValues(_mat, 1, &i_val, 1, &j_val,
		      &petsc_value, INSERT_VALUES);
         CHKERRABORT(libMesh::COMM_WORLD,ierr);
}



template <typename T>
void PetscMatrix<T>::add (const numeric_index_type i,
			  const numeric_index_type j,
			  const T value)
{
  libmesh_assert (this->initialized());

  PetscErrorCode ierr=0;
  PetscInt i_val=i, j_val=j;

  PetscScalar petsc_value = static_cast<PetscScalar>(value);
  ierr = MatSetValues(_mat, 1, &i_val, 1, &j_val,
		      &petsc_value, ADD_VALUES);
         CHKERRABORT(libMesh::COMM_WORLD,ierr);
}



template <typename T>
void PetscMatrix<T>::add_matrix(const DenseMatrix<T>& dm,
				const std::vector<numeric_index_type>& dof_indices)
{
  this->add_matrix (dm, dof_indices, dof_indices);
}







template <typename T>
void PetscMatrix<T>::add (const T a_in, SparseMatrix<T> &X_in)
{
  libmesh_assert (this->initialized());

  // sanity check. but this cannot avoid
  // crash due to incompatible sparsity structure...
  libmesh_assert_equal_to (this->m(), X_in.m());
  libmesh_assert_equal_to (this->n(), X_in.n());

  PetscScalar     a = static_cast<PetscScalar>      (a_in);
  PetscMatrix<T>* X = libmesh_cast_ptr<PetscMatrix<T>*> (&X_in);

  libmesh_assert (X);

  PetscErrorCode ierr=0;

  // the matrix from which we copy the values has to be assembled/closed
  // X->close ();
  libmesh_assert(X->closed());

  semiparallel_only();

// 2.2.x & earlier style
#if PETSC_VERSION_LESS_THAN(2,3,0)

  ierr = MatAXPY(&a,  X->_mat, _mat, SAME_NONZERO_PATTERN);
         CHKERRABORT(libMesh::COMM_WORLD,ierr);

// 2.3.x & newer
#else

  ierr = MatAXPY(_mat, a, X->_mat, DIFFERENT_NONZERO_PATTERN);
         CHKERRABORT(libMesh::COMM_WORLD,ierr);

#endif
}




template <typename T>
T PetscMatrix<T>::operator () (const numeric_index_type i,
			       const numeric_index_type j) const
{
  libmesh_assert (this->initialized());

#if PETSC_VERSION_LESS_THAN(2,2,1)

  // PETSc 2.2.0 & older
  PetscScalar *petsc_row;
  int* petsc_cols;

#else

  // PETSc 2.2.1 & newer
  const PetscScalar *petsc_row;
  const PetscInt    *petsc_cols;

#endif


  // If the entry is not in the sparse matrix, it is 0.
  T value=0.;

  PetscErrorCode
    ierr=0;
  PetscInt
    ncols=0,
    i_val=static_cast<PetscInt>(i),
    j_val=static_cast<PetscInt>(j);


  // the matrix needs to be closed for this to work
  // this->close();
  // but closing it is a semiparallel operation; we want operator()
  // to run on one processor.
  libmesh_assert(this->closed());

  ierr = MatGetRow(_mat, i_val, &ncols, &petsc_cols, &petsc_row);
         CHKERRABORT(libMesh::COMM_WORLD,ierr);

  // Perform a binary search to find the contiguous index in
  // petsc_cols (resp. petsc_row) corresponding to global index j_val
  std::pair<const PetscInt*, const PetscInt*> p =
    std::equal_range (&petsc_cols[0], &petsc_cols[0] + ncols, j_val);

  // Found an entry for j_val
  if (p.first != p.second)
    {
      // The entry in the contiguous row corresponding
      // to the j_val column of interest
      const std::size_t j =
        std::distance (const_cast<PetscInt*>(&petsc_cols[0]),
                       const_cast<PetscInt*>(p.first));

      libmesh_assert_less (static_cast<PetscInt>(j), ncols);
      libmesh_assert_equal_to (petsc_cols[j], j_val);

      value = static_cast<T> (petsc_row[j]);
    }

  ierr  = MatRestoreRow(_mat, i_val,
			&ncols, &petsc_cols, &petsc_row);
          CHKERRABORT(libMesh::COMM_WORLD,ierr);

  return value;
}




template <typename T>
bool PetscMatrix<T>::closed() const
{
  libmesh_assert (this->initialized());

  PetscErrorCode ierr=0;
  PetscBool assembled;

  ierr = MatAssembled(_mat, &assembled);
         CHKERRABORT(libMesh::COMM_WORLD,ierr);

  return (assembled == PETSC_TRUE);
}



template <typename T>
void PetscMatrix<T>::swap(PetscMatrix<T> &m)
{
  std::swap(_mat, m._mat);
  std::swap(_destroy_mat_on_exit, m._destroy_mat_on_exit);
}



//------------------------------------------------------------------
// Explicit instantiations
template class PetscMatrix<Number>;

} // namespace libMesh


#endif // #ifdef LIBMESH_HAVE_PETSC
