// ---------------------------------------------------------------
// DatasetClient.H
// ---------------------------------------------------------------
#ifndef _DATASETCLIENT_H_
#define _DATASETCLIENT_H_

#include <AMReX_REAL.H>
#include <AMReX_FArrayBox.H>
#include <AMReX_MultiFab.H>

#ifdef BL_ARRAYVIEW_TAGBOX
#include <AMReX_TagBox.H>
#endif

using amrex::Real;

extern "C" {

// -------------------------------------------------------------
// "format" is the printf format for numerical display ( ex:  "%5.3f" ).
// "label"  is the first part of the window title ( ex: "myfabname" ).
// "element" is a single element of a MultiFab (which is a fab).
// "nvar" is the number of variables in a Real array.
// "lodim" and "hidim" are array dimensions of size [BL_SPACEDIM]
// -------------------------------------------------------------
// -------------------------------------------------------------
// pointer to fab interface

  bool ArrayView(amrex::FArrayBox *fab);
  bool ArrayViewFab(amrex::FArrayBox *fab);
  bool ArrayViewFabFormatLabel(amrex::FArrayBox *fab, const char *format,
                               const char *label);

// -------------------------------------------------------------
// pointer to MultiFab interface

  // view a single fab from the multifab
  bool ArrayViewMultiFabElement(amrex::MultiFab *multifab, int element);
  bool ArrayViewMultiFabElementFormatLabel(amrex::MultiFab *multifab, int element,
                            const char *format,  const char *label);

  // view the entire multifab
  bool ArrayViewMultiFab(amrex::MultiFab *multifab);
  bool ArrayViewMultiFabFormatLabel(amrex::MultiFab *multifab, const char *format,
                                    const char *label);

// -------------------------------------------------------------
// pointer to real interface

  bool ArrayViewReal(Real *data, const int *lodim, const int *hidim);
  bool ArrayViewRealFormatLabel(Real *data, const int *lodim, const int *hidim,
                           const char *format, const char *label);
  bool ArrayViewRealNVar(Real *data, int nvar, const int *lodim, const int *hidim);
  bool ArrayViewRealNVarFormatLabel(Real *data, int nvar,
                               const int *lodim, const int *hidim,
                               const char *format, const char *label);

#if (BL_SPACEDIM == 2)
  bool ArrayViewRealDims(Real *data, int xlo, int ylo, int xhi, int yhi);
  bool ArrayViewRealDimsFormatLabel(Real *data,
                         int xlo, int ylo, int xhi, int yhi,
                         const char *format, const char *label);

  bool ArrayViewRealNVarDims(Real *data, int nvar,
                         int xlo, int ylo, int xhi, int yhi);
  bool ArrayViewRealNVarDimsFormatLabel(Real *data, int nvar,
                         int xlo, int ylo, int xhi, int yhi,
                         const char *format, const char *label);
#else
  bool ArrayViewRealDims(Real *data, int xlo, int ylo, int zlo,
                         int xhi, int yhi, int zhi);
  bool ArrayViewRealDimsFormatLabel(Real *data, int xlo, int ylo, int zlo,
                         int xhi, int yhi, int zhi,
                         const char *format, const char *label);
  bool ArrayViewRealNVarDims(Real *data, int nvar, int xlo, int ylo, int zlo,
                             int xhi, int yhi, int zhi);
  bool ArrayViewRealNVarDimsFormatLabel(Real *data, int nvar,
                             int xlo, int ylo, int zlo,
                             int xhi, int yhi, int zhi,
                             const char *format, const char *label);
#endif


  bool ArrayViewRealPtrArrayNVarDims(Real *data[], int nvar,  // size nvar
                   const int *lodim, const int *hidim,        // size BL_SPACEDIM
                   const char *format, const char *label);

#ifdef BL_ARRAYVIEW_TAGBOX
// -------------------------------------------------------------
// pointer to TagBox interface

  bool ArrayViewTagBox(amrex::TagBox *tagbox);

// -------------------------------------------------------------
// pointer to TagBoxArray interface

  bool ArrayViewTagBoxArray(amrex::TagBoxArray *tagboxarray);
#endif

};
// -------------------------------------------------------------
// -------------------------------------------------------------
#endif
