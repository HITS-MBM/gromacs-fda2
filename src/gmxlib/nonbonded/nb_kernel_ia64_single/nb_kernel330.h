/* -*- mode: c; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup"; -*- 
 *
 * $Id$
 * 
 * This file is part of Gromacs        Copyright (c) 1991-3304
 * David van der Spoel, Erik Lindahl, University of Groningen.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * To help us fund GROMACS development, we humbly ask that you cite
 * the research papers on the package. Check out http://www.gromacs.org
 * 
 * And Hey:
 * Gnomes, ROck Monsters And Chili Sauce
 */
#ifndef _NB_KERNEL330_H_
#define _NB_KERNEL330_H_

/* This header is never installed, so we can use config.h */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif


/*! \file  nb_kernel330.h
 *  \brief Nonbonded kernel 330 (Coul + LJ, TIP4p-TIP4p)
 *
 *  \internal
 */

#ifdef __cplusplus
extern "C" {
#endif
#if 0
}
#endif
#define F77_OR_C_FUNC_(a,A) a


/*! \brief Nonbonded kernel 330 with forces.
 *
 *  \internal  Generated at compile time in either C or Fortran
 *             depending on configuration settings. The name of
 *             the function in C is nb_kernel330. For Fortran the
 *             name mangling depends on the compiler, but in Gromacs
 *             you can handle it automatically with the macro
 *             F77_OR_C_FUNC_(nb_kernel330,NB_KERNEL330), which
 *             expands to the correct identifier.
 *
 *  <b>Coulomb interaction:</b> Standard 1/r <br>
 *  <b>VdW interaction:</b> Lennard-Jones <br>
 *  <b>Water optimization:</b> TIP4p - TIP4p <br>
 *  <b>Forces calculated:</b> Yes <br>
 *
 *  \note All level1 and level2 nonbonded kernels use the same
 *        call sequence. Parameters are documented in nb_kernel.h
 */
void
F77_OR_C_FUNC_(nb_kernel330,NB_KERNEL330)
                (int *         nri,        int           iinr[],     
                 int           jindex[],   int           jjnr[],   
                 int           shift[],    float          shiftvec[],
                 float          fshift[],   int           gid[], 
                 float          pos[],      float          faction[],
                 float          charge[],   float *        facel,
                 float *        krf,        float *        crf,  
                 float          Vc[],       int           type[],   
                 int *         ntype,      float          vdwparam[],
                 float          Vvdw[],     float *        tabscale,
                 float          VFtab[],    float          invsqrta[], 
                 float          dvda[],     float *        gbtabscale,
                 float          GBtab[],    int *         nthreads, 
                 int *         count,      void *        mtx,
                 int *         outeriter,  int *         inneriter,
                 float          work[]);


/*! \brief Nonbonded kernel 330 without forces.
 *
 *  \internal  Generated at compile time in either C or Fortran
 *             depending on configuration settings. The name of
 *             the function in C is nb_kernel330. For Fortran the
 *             name mangling depends on the compiler, but in Gromacs
 *             you can handle it automatically with the macro
 *             F77_OR_C_FUNC_(nb_kernel330,NB_KERNEL330), which
 *             expands to the correct identifier.
 *
 *  <b>Coulomb interaction:</b> Standard 1/r <br>
 *  <b>VdW interaction:</b> Lennard-Jones <br>
 *  <b>Water optimization:</b> TIP4p - TIP4p <br>
 *  <b>Forces calculated:</b> No <br>
 *
 *  \note All level1 and level2 nonbonded kernels use the same
 *        call sequence. Parameters are documented in nb_kernel.h
 */
void
F77_OR_C_FUNC_(nb_kernel330nf,NB_KERNEL330NF)
                (int *         nri,        int           iinr[],     
                 int           jindex[],   int           jjnr[],   
                 int           shift[],    float          shiftvec[],
                 float          fshift[],   int           gid[], 
                 float          pos[],      float          faction[],
                 float          charge[],   float *        facel,
                 float *        krf,        float *        crf,  
                 float          Vc[],       int           type[],   
                 int *         ntype,      float          vdwparam[],
                 float          Vvdw[],     float *        tabscale,
                 float          VFtab[],    float          invsqrta[], 
                 float          dvda[],     float *        gbtabscale,
                 float          GBtab[],    int *         nthreads, 
                 int *         count,      void *        mtx,
                 int *         outeriter,  int *         inneriter,
                 float          work[]);


#ifdef __cplusplus
}
#endif

#endif /* _NB_KERNEL330_H_ */
