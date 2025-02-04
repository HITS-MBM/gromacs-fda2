/*
 * This file is part of the GROMACS molecular simulation package.
 *
 * Copyright (c) 2008,2009,2010, The GROMACS development team.
 * Copyright (c) 2012,2013,2014,2015,2016 The GROMACS development team.
 * Copyright (c) 2017,2018,2019,2020,2021, by the GROMACS development team, led by
 * Mark Abraham, David van der Spoel, Berk Hess, and Erik Lindahl,
 * and including many others, as listed in the AUTHORS file in the
 * top-level source directory and at http://www.gromacs.org.
 *
 * GROMACS is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1
 * of the License, or (at your option) any later version.
 *
 * GROMACS is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with GROMACS; if not, see
 * http://www.gnu.org/licenses, or write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA.
 *
 * If you want to redistribute modifications to GROMACS, please
 * consider that scientific software is very special. Version
 * control is crucial - bugs must be traceable. We will be happy to
 * consider code for inclusion in the official distribution, but
 * derived work must not be called official GROMACS. Details are found
 * in the README & COPYING files - if they are missing, get the
 * official version at http://www.gromacs.org.
 *
 * To help us fund GROMACS development, we humbly ask that you cite
 * the research papers on the package. Check out http://www.gromacs.org.
 */
#include "gmxpre.h"

#include "mtop_util.h"

#include <climits>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "gromacs/math/vectypes.h"
#include "gromacs/topology/atoms.h"
#include "gromacs/topology/block.h"
#include "gromacs/topology/exclusionblocks.h"
#include "gromacs/topology/idef.h"
#include "gromacs/topology/ifunc.h"
#include "gromacs/topology/topology.h"
#include "gromacs/topology/topsort.h"
#include "gromacs/utility/arrayref.h"
#include "gromacs/utility/fatalerror.h"
#include "gromacs/utility/real.h"
#include "gromacs/utility/smalloc.h"

void gmx_mtop_count_atomtypes(const gmx_mtop_t* mtop, int state, int typecount[])
{
    for (int i = 0; i < mtop->ffparams.atnr; ++i)
    {
        typecount[i] = 0;
    }
    for (const gmx_molblock_t& molb : mtop->molblock)
    {
        const t_atoms& atoms = mtop->moltype[molb.type].atoms;
        for (int i = 0; i < atoms.nr; ++i)
        {
            int tpi;
            if (state == 0)
            {
                tpi = atoms.atom[i].type;
            }
            else
            {
                tpi = atoms.atom[i].typeB;
            }
            typecount[tpi] += molb.nmol;
        }
    }
}

int gmx_mtop_num_molecules(const gmx_mtop_t& mtop)
{
    int numMolecules = 0;
    for (const gmx_molblock_t& molb : mtop.molblock)
    {
        numMolecules += molb.nmol;
    }
    return numMolecules;
}

int gmx_mtop_nres(const gmx_mtop_t* mtop)
{
    int nres = 0;
    for (const gmx_molblock_t& molb : mtop->molblock)
    {
        nres += molb.nmol * mtop->moltype[molb.type].atoms.nres;
    }
    return nres;
}

AtomIterator::AtomIterator(const gmx_mtop_t& mtop, int globalAtomNumber) :
    mtop_(&mtop),
    mblock_(0),
    atoms_(&mtop.moltype[mtop.molblock[0].type].atoms),
    currentMolecule_(0),
    highestResidueNumber_(mtop.maxResNumberNotRenumbered()),
    localAtomNumber_(0),
    globalAtomNumber_(globalAtomNumber)
{
    GMX_ASSERT(globalAtomNumber == 0 || globalAtomNumber == mtop.natoms,
               "Starting at other atoms not implemented yet");
}

AtomIterator& AtomIterator::operator++()
{
    localAtomNumber_++;
    globalAtomNumber_++;

    if (localAtomNumber_ >= atoms_->nr)
    {
        if (atoms_->nres <= mtop_->maxResiduesPerMoleculeToTriggerRenumber())
        {
            /* Single residue molecule, increase the count with one */
            highestResidueNumber_ += atoms_->nres;
        }
        currentMolecule_++;
        localAtomNumber_ = 0;
        if (currentMolecule_ >= mtop_->molblock[mblock_].nmol)
        {
            mblock_++;
            if (mblock_ >= mtop_->molblock.size())
            {
                return *this;
            }
            atoms_           = &mtop_->moltype[mtop_->molblock[mblock_].type].atoms;
            currentMolecule_ = 0;
        }
    }
    return *this;
}

bool AtomIterator::operator==(const AtomIterator& o) const
{
    return mtop_ == o.mtop_ && globalAtomNumber_ == o.globalAtomNumber_;
}

const t_atom& AtomProxy::atom() const
{
    return it_->atoms_->atom[it_->localAtomNumber_];
}

int AtomProxy::globalAtomNumber() const
{
    return it_->globalAtomNumber_;
}

const char* AtomProxy::atomName() const
{
    return *(it_->atoms_->atomname[it_->localAtomNumber_]);
}

const char* AtomProxy::residueName() const
{
    int residueIndexInMolecule = it_->atoms_->atom[it_->localAtomNumber_].resind;
    return *(it_->atoms_->resinfo[residueIndexInMolecule].name);
}

int AtomProxy::residueNumber() const
{
    int residueIndexInMolecule = it_->atoms_->atom[it_->localAtomNumber_].resind;
    if (it_->atoms_->nres <= it_->mtop_->maxResiduesPerMoleculeToTriggerRenumber())
    {
        return it_->highestResidueNumber_ + 1 + residueIndexInMolecule;
    }
    else
    {
        return it_->atoms_->resinfo[residueIndexInMolecule].nr;
    }
}

const gmx_moltype_t& AtomProxy::moleculeType() const
{
    return it_->mtop_->moltype[it_->mtop_->molblock[it_->mblock_].type];
}

int AtomProxy::atomNumberInMol() const
{
    return it_->localAtomNumber_;
}

typedef struct gmx_mtop_atomloop_block
{
    const gmx_mtop_t* mtop;
    size_t            mblock;
    const t_atoms*    atoms;
    int               at_local;
} t_gmx_mtop_atomloop_block;

gmx_mtop_atomloop_block_t gmx_mtop_atomloop_block_init(const gmx_mtop_t* mtop)
{
    struct gmx_mtop_atomloop_block* aloop;

    snew(aloop, 1);

    aloop->mtop     = mtop;
    aloop->mblock   = 0;
    aloop->atoms    = &mtop->moltype[mtop->molblock[aloop->mblock].type].atoms;
    aloop->at_local = -1;

    return aloop;
}

static void gmx_mtop_atomloop_block_destroy(gmx_mtop_atomloop_block_t aloop)
{
    sfree(aloop);
}

gmx_bool gmx_mtop_atomloop_block_next(gmx_mtop_atomloop_block_t aloop, const t_atom** atom, int* nmol)
{
    if (aloop == nullptr)
    {
        gmx_incons("gmx_mtop_atomloop_all_next called without calling gmx_mtop_atomloop_all_init");
    }

    aloop->at_local++;

    if (aloop->at_local >= aloop->atoms->nr)
    {
        aloop->mblock++;
        if (aloop->mblock >= aloop->mtop->molblock.size())
        {
            gmx_mtop_atomloop_block_destroy(aloop);
            return FALSE;
        }
        aloop->atoms    = &aloop->mtop->moltype[aloop->mtop->molblock[aloop->mblock].type].atoms;
        aloop->at_local = 0;
    }

    *atom = &aloop->atoms->atom[aloop->at_local];
    *nmol = aloop->mtop->molblock[aloop->mblock].nmol;

    return TRUE;
}

typedef struct gmx_mtop_ilistloop
{
    const gmx_mtop_t* mtop;
    int               mblock;
} t_gmx_mtop_ilist;

gmx_mtop_ilistloop_t gmx_mtop_ilistloop_init(const gmx_mtop_t* mtop)
{
    struct gmx_mtop_ilistloop* iloop;

    snew(iloop, 1);

    iloop->mtop   = mtop;
    iloop->mblock = -1;

    return iloop;
}

gmx_mtop_ilistloop_t gmx_mtop_ilistloop_init(const gmx_mtop_t& mtop)
{
    return gmx_mtop_ilistloop_init(&mtop);
}

static void gmx_mtop_ilistloop_destroy(gmx_mtop_ilistloop_t iloop)
{
    sfree(iloop);
}

const InteractionLists* gmx_mtop_ilistloop_next(gmx_mtop_ilistloop_t iloop, int* nmol)
{
    if (iloop == nullptr)
    {
        gmx_incons("gmx_mtop_ilistloop_next called without calling gmx_mtop_ilistloop_init");
    }

    iloop->mblock++;
    if (iloop->mblock >= gmx::ssize(iloop->mtop->molblock))
    {
        if (iloop->mblock == gmx::ssize(iloop->mtop->molblock) && iloop->mtop->bIntermolecularInteractions)
        {
            *nmol = 1;
            return iloop->mtop->intermolecular_ilist.get();
        }

        gmx_mtop_ilistloop_destroy(iloop);
        return nullptr;
    }

    *nmol = iloop->mtop->molblock[iloop->mblock].nmol;

    return &iloop->mtop->moltype[iloop->mtop->molblock[iloop->mblock].type].ilist;
}
typedef struct gmx_mtop_ilistloop_all
{
    const gmx_mtop_t* mtop;
    size_t            mblock;
    int               mol;
    int               a_offset;
} t_gmx_mtop_ilist_all;

int gmx_mtop_ftype_count(const gmx_mtop_t* mtop, int ftype)
{
    gmx_mtop_ilistloop_t iloop;
    int                  n, nmol;

    n = 0;

    iloop = gmx_mtop_ilistloop_init(mtop);
    while (const InteractionLists* il = gmx_mtop_ilistloop_next(iloop, &nmol))
    {
        n += nmol * (*il)[ftype].size() / (1 + NRAL(ftype));
    }

    if (mtop->bIntermolecularInteractions)
    {
        n += (*mtop->intermolecular_ilist)[ftype].size() / (1 + NRAL(ftype));
    }

    return n;
}

int gmx_mtop_ftype_count(const gmx_mtop_t& mtop, int ftype)
{
    return gmx_mtop_ftype_count(&mtop, ftype);
}

int gmx_mtop_interaction_count(const gmx_mtop_t& mtop, const int unsigned if_flags)
{
    int n = 0;

    gmx_mtop_ilistloop_t iloop = gmx_mtop_ilistloop_init(mtop);
    int                  nmol;
    while (const InteractionLists* il = gmx_mtop_ilistloop_next(iloop, &nmol))
    {
        for (int ftype = 0; ftype < F_NRE; ftype++)
        {
            if ((interaction_function[ftype].flags & if_flags) == if_flags)
            {
                n += nmol * (*il)[ftype].size() / (1 + NRAL(ftype));
            }
        }
    }

    if (mtop.bIntermolecularInteractions)
    {
        for (int ftype = 0; ftype < F_NRE; ftype++)
        {
            if ((interaction_function[ftype].flags & if_flags) == if_flags)
            {
                n += (*mtop.intermolecular_ilist)[ftype].size() / (1 + NRAL(ftype));
            }
        }
    }

    return n;
}

std::array<int, eptNR> gmx_mtop_particletype_count(const gmx_mtop_t& mtop)
{
    std::array<int, eptNR> count = { { 0 } };

    for (const auto& molblock : mtop.molblock)
    {
        const t_atoms& atoms = mtop.moltype[molblock.type].atoms;
        for (int a = 0; a < atoms.nr; a++)
        {
            count[atoms.atom[a].ptype] += molblock.nmol;
        }
    }

    return count;
}

static void atomcat(t_atoms* dest, const t_atoms* src, int copies, int maxres_renum, int* maxresnr)
{
    int i, j, l, size;
    int srcnr  = src->nr;
    int destnr = dest->nr;

    if (dest->nr == 0)
    {
        dest->haveMass    = src->haveMass;
        dest->haveType    = src->haveType;
        dest->haveCharge  = src->haveCharge;
        dest->haveBState  = src->haveBState;
        dest->havePdbInfo = src->havePdbInfo;
    }
    else
    {
        dest->haveMass    = dest->haveMass && src->haveMass;
        dest->haveType    = dest->haveType && src->haveType;
        dest->haveCharge  = dest->haveCharge && src->haveCharge;
        dest->haveBState  = dest->haveBState && src->haveBState;
        dest->havePdbInfo = dest->havePdbInfo && src->havePdbInfo;
    }

    if (srcnr)
    {
        size = destnr + copies * srcnr;
        srenew(dest->atom, size);
        srenew(dest->atomname, size);
        if (dest->haveType)
        {
            srenew(dest->atomtype, size);
            if (dest->haveBState)
            {
                srenew(dest->atomtypeB, size);
            }
        }
        if (dest->havePdbInfo)
        {
            srenew(dest->pdbinfo, size);
        }
    }
    if (src->nres)
    {
        size = dest->nres + copies * src->nres;
        srenew(dest->resinfo, size);
    }

    /* residue information */
    for (l = dest->nres, j = 0; (j < copies); j++, l += src->nres)
    {
        memcpy(reinterpret_cast<char*>(&(dest->resinfo[l])), reinterpret_cast<char*>(&(src->resinfo[0])),
               static_cast<size_t>(src->nres * sizeof(src->resinfo[0])));
    }

    for (l = destnr, j = 0; (j < copies); j++, l += srcnr)
    {
        memcpy(reinterpret_cast<char*>(&(dest->atom[l])), reinterpret_cast<char*>(&(src->atom[0])),
               static_cast<size_t>(srcnr * sizeof(src->atom[0])));
        memcpy(reinterpret_cast<char*>(&(dest->atomname[l])),
               reinterpret_cast<char*>(&(src->atomname[0])),
               static_cast<size_t>(srcnr * sizeof(src->atomname[0])));
        if (dest->haveType)
        {
            memcpy(reinterpret_cast<char*>(&(dest->atomtype[l])),
                   reinterpret_cast<char*>(&(src->atomtype[0])),
                   static_cast<size_t>(srcnr * sizeof(src->atomtype[0])));
            if (dest->haveBState)
            {
                memcpy(reinterpret_cast<char*>(&(dest->atomtypeB[l])),
                       reinterpret_cast<char*>(&(src->atomtypeB[0])),
                       static_cast<size_t>(srcnr * sizeof(src->atomtypeB[0])));
            }
        }
        if (dest->havePdbInfo)
        {
            memcpy(reinterpret_cast<char*>(&(dest->pdbinfo[l])),
                   reinterpret_cast<char*>(&(src->pdbinfo[0])),
                   static_cast<size_t>(srcnr * sizeof(src->pdbinfo[0])));
        }
    }

    /* Increment residue indices */
    for (l = destnr, j = 0; (j < copies); j++)
    {
        for (i = 0; (i < srcnr); i++, l++)
        {
            dest->atom[l].resind = dest->nres + j * src->nres + src->atom[i].resind;
        }
    }

    if (src->nres <= maxres_renum)
    {
        /* Single residue molecule, continue counting residues */
        for (j = 0; (j < copies); j++)
        {
            for (l = 0; l < src->nres; l++)
            {
                (*maxresnr)++;
                dest->resinfo[dest->nres + j * src->nres + l].nr = *maxresnr;
            }
        }
    }

    dest->nres += copies * src->nres;
    dest->nr += copies * src->nr;
}

t_atoms gmx_mtop_global_atoms(const gmx_mtop_t* mtop)
{
    t_atoms atoms;

    init_t_atoms(&atoms, 0, FALSE);

    int maxresnr = mtop->maxResNumberNotRenumbered();
    for (const gmx_molblock_t& molb : mtop->molblock)
    {
        atomcat(&atoms, &mtop->moltype[molb.type].atoms, molb.nmol,
                mtop->maxResiduesPerMoleculeToTriggerRenumber(), &maxresnr);
    }

    return atoms;
}

/*
 * The cat routines below are old code from src/kernel/topcat.c
 */

static void ilistcat(int ftype, InteractionList* dest, const InteractionList& src, int copies, int dnum, int snum)
{
    int nral, c, i, a;

    nral = NRAL(ftype);

    size_t destIndex = dest->iatoms.size();
    dest->iatoms.resize(dest->iatoms.size() + copies * src.size());

    for (c = 0; c < copies; c++)
    {
        for (i = 0; i < src.size();)
        {
            dest->iatoms[destIndex++] = src.iatoms[i++];
            for (a = 0; a < nral; a++)
            {
                dest->iatoms[destIndex++] = dnum + src.iatoms[i++];
            }
        }
        dnum += snum;
    }
}

static void ilistcat(int ftype, t_ilist* dest, const InteractionList& src, int copies, int dnum, int snum)
{
    int nral, c, i, a;

    nral = NRAL(ftype);

    dest->nalloc = dest->nr + copies * src.size();
    srenew(dest->iatoms, dest->nalloc);

    for (c = 0; c < copies; c++)
    {
        for (i = 0; i < src.size();)
        {
            dest->iatoms[dest->nr++] = src.iatoms[i++];
            for (a = 0; a < nral; a++)
            {
                dest->iatoms[dest->nr++] = dnum + src.iatoms[i++];
            }
        }
        dnum += snum;
    }
}

static void pf_ilistcat(int                     ftype,
		                InteractionList        *dest,
						const InteractionList  &src,
						int                     copies,
                        int                     dnum,
						int                     snum,
						fda::FDASettings const &fda_settings)
{
	// Return if no bonded interaction is needed.
	if (!(fda_settings.type & (fda::InteractionType_BONDED + fda::InteractionType_NB14))) return;

    int nral, c, i, a, atomIdx;
    char needed;

    nral = NRAL(ftype);

    t_iatom *tmp;
    snew(tmp, copies*src.size());
    int len = 0;

    int *g1atomsBeg = fda_settings.groups->a + fda_settings.groups->index[fda_settings.index_group1];
    int *g1atomsEnd = fda_settings.groups->a + fda_settings.groups->index[fda_settings.index_group1 + 1];
    int *g1atomsCur = nullptr;
    int *g2atomsBeg = fda_settings.groups->a + fda_settings.groups->index[fda_settings.index_group2];
    int *g2atomsEnd = fda_settings.groups->a + fda_settings.groups->index[fda_settings.index_group2 + 1];
    int *g2atomsCur = nullptr;

    for (c = 0; c < copies; c++)
    {
        for (i = 0; i < src.size(); )
        {
            needed = 0;
            for (a = 0; a < nral; a++)
            {
            	atomIdx = dnum + src.iatoms[i+a+1];
        		for (g1atomsCur = g1atomsBeg; g1atomsCur < g1atomsEnd; ++g1atomsCur) {
        			if (atomIdx == *g1atomsCur) needed = 1;
        		}
        		for (g2atomsCur = g2atomsBeg; g2atomsCur < g2atomsEnd; ++g2atomsCur) {
        			if (atomIdx == *g2atomsCur) needed = 1;
        		}
            }
            if (needed) {
                tmp[len++] = src.iatoms[i];
                for (a = 0; a < nral; a++) tmp[len++] = dnum + src.iatoms[i+a+1];

                #ifdef FDA_BONDEXCL_PRINT_DEBUG_ON
					fprintf(stderr, "=== DEBUG === bonded interaction %i", ftype);
					fprintf(stderr, " %i", src->iatoms[i]);
					for (a = 0; a < nral; a++) fprintf(stderr, " %i", dnum + src->iatoms[i + a + 1]);
					fprintf(stderr, " needed\n");
					fflush(stderr);
                #endif
            } else {
                #ifdef FDA_BONDEXCL_PRINT_DEBUG_ON
					fprintf(stderr, "=== DEBUG === bonded interaction %i", ftype);
					fprintf(stderr, " %i", src->iatoms[i]);
					for (a = 0; a < nral; a++) fprintf(stderr, " %i", dnum + src->iatoms[i + a + 1]);
					fprintf(stderr, " not needed\n");
					fflush(stderr);
                #endif
            }
            i += a + 1;
        }
        dnum += snum;
    }

    size_t destIndex = dest->iatoms.size();
    dest->iatoms.resize(dest->iatoms.size() + len);

    for (i = 0; i < len; i++) dest->iatoms[destIndex++] = tmp[i];

    sfree(tmp);
}

static void pf_ilistcat(int                     ftype,
		                t_ilist                *dest,
						const InteractionList  &src,
						int                     copies,
                        int                     dnum,
						int                     snum,
						fda::FDASettings const &fda_settings)
{
	// Return if no bonded interaction is needed.
	if (!(fda_settings.type & (fda::InteractionType_BONDED + fda::InteractionType_NB14))) return;

    int nral, c, i, a, atomIdx;
    char needed;

    nral = NRAL(ftype);

    t_iatom *tmp;
    snew(tmp, copies*src.size());
    int len = 0;

    int *g1atomsBeg = fda_settings.groups->a + fda_settings.groups->index[fda_settings.index_group1];
    int *g1atomsEnd = fda_settings.groups->a + fda_settings.groups->index[fda_settings.index_group1 + 1];
    int *g1atomsCur = nullptr;
    int *g2atomsBeg = fda_settings.groups->a + fda_settings.groups->index[fda_settings.index_group2];
    int *g2atomsEnd = fda_settings.groups->a + fda_settings.groups->index[fda_settings.index_group2 + 1];
    int *g2atomsCur = nullptr;

    for (c = 0; c < copies; c++)
    {
        for (i = 0; i < src.size(); )
        {
            needed = 0;
            for (a = 0; a < nral; a++)
            {
            	atomIdx = dnum + src.iatoms[i+a+1];
        		for (g1atomsCur = g1atomsBeg; g1atomsCur < g1atomsEnd; ++g1atomsCur) {
        			if (atomIdx == *g1atomsCur) needed = 1;
        		}
        		for (g2atomsCur = g2atomsBeg; g2atomsCur < g2atomsEnd; ++g2atomsCur) {
        			if (atomIdx == *g2atomsCur) needed = 1;
        		}
            }
            if (needed) {
                tmp[len++] = src.iatoms[i];
                for (a = 0; a < nral; a++) tmp[len++] = dnum + src.iatoms[i+a+1];

                #ifdef FDA_BONDEXCL_PRINT_DEBUG_ON
					fprintf(stderr, "=== DEBUG === bonded interaction %i", ftype);
					fprintf(stderr, " %i", src->iatoms[i]);
					for (a = 0; a < nral; a++) fprintf(stderr, " %i", dnum + src->iatoms[i + a + 1]);
					fprintf(stderr, " needed\n");
					fflush(stderr);
                #endif
            } else {
                #ifdef FDA_BONDEXCL_PRINT_DEBUG_ON
					fprintf(stderr, "=== DEBUG === bonded interaction %i", ftype);
					fprintf(stderr, " %i", src->iatoms[i]);
					for (a = 0; a < nral; a++) fprintf(stderr, " %i", dnum + src->iatoms[i + a + 1]);
					fprintf(stderr, " not needed\n");
					fflush(stderr);
                #endif
            }
            i += a + 1;
        }
        dnum += snum;
    }

    dest->nalloc = dest->nr + len;
    srenew(dest->iatoms, dest->nalloc);

    for (i = 0; i < len; i++) dest->iatoms[dest->nr++] = tmp[i];

    sfree(tmp);
}

static const t_iparams& getIparams(const InteractionDefinitions& idef, const int index)
{
    return idef.iparams[index];
}

static const t_iparams& getIparams(const t_idef& idef, const int index)
{
    return idef.iparams[index];
}

static void resizeIParams(std::vector<t_iparams>* iparams, const int newSize)
{
    iparams->resize(newSize);
}

static void resizeIParams(t_iparams** iparams, const int newSize)
{
    srenew(*iparams, newSize);
}

template<typename IdefType>
static void set_posres_params(IdefType* idef, const gmx_molblock_t* molb, int i0, int a_offset)
{
    int        i1, i, a_molb;
    t_iparams* ip;

    auto* il = &idef->il[F_POSRES];
    i1       = il->size() / 2;
    resizeIParams(&idef->iparams_posres, i1);
    for (i = i0; i < i1; i++)
    {
        ip = &idef->iparams_posres[i];
        /* Copy the force constants */
        *ip    = getIparams(*idef, il->iatoms[i * 2]);
        a_molb = il->iatoms[i * 2 + 1] - a_offset;
        if (molb->posres_xA.empty())
        {
            gmx_incons("Position restraint coordinates are missing");
        }
        ip->posres.pos0A[XX] = molb->posres_xA[a_molb][XX];
        ip->posres.pos0A[YY] = molb->posres_xA[a_molb][YY];
        ip->posres.pos0A[ZZ] = molb->posres_xA[a_molb][ZZ];
        if (!molb->posres_xB.empty())
        {
            ip->posres.pos0B[XX] = molb->posres_xB[a_molb][XX];
            ip->posres.pos0B[YY] = molb->posres_xB[a_molb][YY];
            ip->posres.pos0B[ZZ] = molb->posres_xB[a_molb][ZZ];
        }
        else
        {
            ip->posres.pos0B[XX] = ip->posres.pos0A[XX];
            ip->posres.pos0B[YY] = ip->posres.pos0A[YY];
            ip->posres.pos0B[ZZ] = ip->posres.pos0A[ZZ];
        }
        /* Set the parameter index for idef->iparams_posre */
        il->iatoms[i * 2] = i;
    }
}

template<typename IdefType>
static void set_fbposres_params(IdefType* idef, const gmx_molblock_t* molb, int i0, int a_offset)
{
    int        i1, i, a_molb;
    t_iparams* ip;

    auto* il = &idef->il[F_FBPOSRES];
    i1       = il->size() / 2;
    resizeIParams(&idef->iparams_fbposres, i1);
    for (i = i0; i < i1; i++)
    {
        ip = &idef->iparams_fbposres[i];
        /* Copy the force constants */
        *ip    = getIparams(*idef, il->iatoms[i * 2]);
        a_molb = il->iatoms[i * 2 + 1] - a_offset;
        if (molb->posres_xA.empty())
        {
            gmx_incons("Position restraint coordinates are missing");
        }
        /* Take flat-bottom posres reference from normal position restraints */
        ip->fbposres.pos0[XX] = molb->posres_xA[a_molb][XX];
        ip->fbposres.pos0[YY] = molb->posres_xA[a_molb][YY];
        ip->fbposres.pos0[ZZ] = molb->posres_xA[a_molb][ZZ];
        /* Note: no B-type for flat-bottom posres */

        /* Set the parameter index for idef->iparams_posre */
        il->iatoms[i * 2] = i;
    }
}

/*! \brief Copy parameters to idef structure from mtop.
 *
 * Makes a deep copy of the force field parameters data structure from a gmx_mtop_t.
 * Used to initialize legacy topology types.
 *
 * \param[in] mtop Reference to input mtop.
 * \param[in] idef Pointer to idef to populate.
 */
static void copyFFParametersFromMtop(const gmx_mtop_t& mtop, t_idef* idef)
{
    const gmx_ffparams_t* ffp = &mtop.ffparams;

    idef->ntypes = ffp->numTypes();
    idef->atnr   = ffp->atnr;
    /* we can no longer copy the pointers to the mtop members,
     * because they will become invalid as soon as mtop gets free'd.
     * We also need to make sure to only operate on valid data!
     */

    if (!ffp->functype.empty())
    {
        snew(idef->functype, ffp->functype.size());
        std::copy(ffp->functype.data(), ffp->functype.data() + ffp->functype.size(), idef->functype);
    }
    else
    {
        idef->functype = nullptr;
    }
    if (!ffp->iparams.empty())
    {
        snew(idef->iparams, ffp->iparams.size());
        std::copy(ffp->iparams.data(), ffp->iparams.data() + ffp->iparams.size(), idef->iparams);
    }
    else
    {
        idef->iparams = nullptr;
    }
    idef->iparams_posres   = nullptr;
    idef->iparams_fbposres = nullptr;
    idef->fudgeQQ          = ffp->fudgeQQ;
    idef->ilsort           = ilsortUNKNOWN;
}

/*! \brief Copy idef structure from mtop.
 *
 * Makes a deep copy of an idef data structure from a gmx_mtop_t.
 * Used to initialize legacy topology types.
 *
 * \param[in] mtop Reference to input mtop.
 * \param[in] idef Pointer to idef to populate.
 * \param[in] mergeConstr Decide if constraints will be merged.
 */
template<typename IdefType>
static void copyIListsFromMtop(const gmx_mtop_t& mtop, IdefType* idef, bool mergeConstr, fda::FDASettings* ptr_fda_settings)
{
    int natoms = 0;
    for (const gmx_molblock_t& molb : mtop.molblock)
    {
        const gmx_moltype_t& molt = mtop.moltype[molb.type];

        int srcnr  = molt.atoms.nr;
        int destnr = natoms;

        int nposre_old   = idef->il[F_POSRES].size();
        int nfbposre_old = idef->il[F_FBPOSRES].size();
        for (int ftype = 0; ftype < F_NRE; ftype++)
        {
            if (mergeConstr && ftype == F_CONSTR && !molt.ilist[F_CONSTRNC].empty())
            {
                /* Merge all constrains into one ilist.
                 * This simplifies the constraint code.
                 */
                for (int mol = 0; mol < molb.nmol; mol++)
                {
                    ilistcat(ftype, &idef->il[F_CONSTR], molt.ilist[F_CONSTR], 1,
                             destnr + mol * srcnr, srcnr);
                    ilistcat(ftype, &idef->il[F_CONSTR], molt.ilist[F_CONSTRNC], 1,
                             destnr + mol * srcnr, srcnr);
                }
            }
            else if (!(mergeConstr && ftype == F_CONSTRNC))
            {
            	if (ptr_fda_settings and ptr_fda_settings->bonded_exclusion_on)
                    pf_ilistcat(ftype, &idef->il[ftype], molt.ilist[ftype],
                             molb.nmol, destnr, srcnr, *ptr_fda_settings);
            	else
                    ilistcat(ftype, &idef->il[ftype], molt.ilist[ftype],
                             molb.nmol, destnr, srcnr);
            }
        }
        if (idef->il[F_POSRES].size() > nposre_old)
        {
            /* Executing this line line stops gmxdump -sys working
             * correctly. I'm not aware there's an elegant fix. */
            set_posres_params(idef, &molb, nposre_old / 2, natoms);
        }
        if (idef->il[F_FBPOSRES].size() > nfbposre_old)
        {
            set_fbposres_params(idef, &molb, nfbposre_old / 2, natoms);
        }

        natoms += molb.nmol * srcnr;
    }

    if (mtop.bIntermolecularInteractions)
    {
        for (int ftype = 0; ftype < F_NRE; ftype++)
        {
            ilistcat(ftype, &idef->il[ftype], (*mtop.intermolecular_ilist)[ftype], 1, 0, mtop.natoms);
        }
    }

    // We have not (yet) sorted free-energy interactions to the end of the ilists
    idef->ilsort = ilsortNO_FE;
}

/*! \brief Copy atomtypes from mtop
 *
 * Makes a deep copy of t_atomtypes from gmx_mtop_t.
 * Used to initialize legacy topology types.
 *
 * \param[in] mtop Reference to input mtop.
 * \param[in] atomtypes Pointer to atomtypes to populate.
 */
static void copyAtomtypesFromMtop(const gmx_mtop_t& mtop, t_atomtypes* atomtypes)
{
    atomtypes->nr = mtop.atomtypes.nr;
    if (mtop.atomtypes.atomnumber)
    {
        snew(atomtypes->atomnumber, mtop.atomtypes.nr);
        std::copy(mtop.atomtypes.atomnumber, mtop.atomtypes.atomnumber + mtop.atomtypes.nr,
                  atomtypes->atomnumber);
    }
    else
    {
        atomtypes->atomnumber = nullptr;
    }
}

/*! \brief Generate a single list of lists of exclusions for the whole system
 *
 * \param[in] mtop  Reference to input mtop.
 */
static gmx::ListOfLists<int> globalExclusionLists(const gmx_mtop_t& mtop)
{
    gmx::ListOfLists<int> excls;

    int atomIndex = 0;
    for (const gmx_molblock_t& molb : mtop.molblock)
    {
        const gmx_moltype_t& molt = mtop.moltype[molb.type];

        for (int mol = 0; mol < molb.nmol; mol++)
        {
            excls.appendListOfLists(molt.excls, atomIndex);

            atomIndex += molt.atoms.nr;
        }
    }

    return excls;
}

/*! \brief Updates inter-molecular exclusion lists
 *
 * This function updates inter-molecular exclusions to exclude all
 * non-bonded interactions between a given list of atoms
 *
 * \param[inout]    excls   existing exclusions in local topology
 * \param[in]       ids     list of global IDs of atoms
 */
static void addMimicExclusions(gmx::ListOfLists<int>* excls, const gmx::ArrayRef<const int> ids)
{
    t_blocka inter_excl{};
    init_blocka(&inter_excl);
    size_t n_q = ids.size();

    inter_excl.nr  = excls->ssize();
    inter_excl.nra = n_q * n_q;

    size_t total_nra = n_q * n_q;

    snew(inter_excl.index, excls->ssize() + 1);
    snew(inter_excl.a, total_nra);

    for (int i = 0; i < inter_excl.nr; ++i)
    {
        inter_excl.index[i] = 0;
    }

    /* Here we loop over the list of QM atom ids
     *  and create exclusions between all of them resulting in
     *  n_q * n_q sized exclusion list
     */
    int prev_index = 0;
    for (int k = 0; k < inter_excl.nr; ++k)
    {
        inter_excl.index[k] = prev_index;
        for (long i = 0; i < ids.ssize(); i++)
        {
            if (k != ids[i])
            {
                continue;
            }
            size_t index             = n_q * i;
            inter_excl.index[ids[i]] = index;
            prev_index               = index + n_q;
            for (size_t j = 0; j < n_q; ++j)
            {
                inter_excl.a[n_q * i + j] = ids[j];
            }
        }
    }
    inter_excl.index[ids[n_q - 1] + 1] = n_q * n_q;

    inter_excl.index[inter_excl.nr] = n_q * n_q;

    std::vector<gmx::ExclusionBlock> qmexcl2(excls->size());
    gmx::blockaToExclusionBlocks(&inter_excl, qmexcl2);

    // Merge the created exclusion list with the existing one
    gmx::mergeExclusions(excls, qmexcl2);
}

static void sortFreeEnergyInteractionsAtEnd(const gmx_mtop_t& mtop, InteractionDefinitions* idef)
{
    std::vector<real> qA(mtop.natoms);
    std::vector<real> qB(mtop.natoms);
    for (const AtomProxy atomP : AtomRange(mtop))
    {
        const t_atom& local = atomP.atom();
        int           index = atomP.globalAtomNumber();
        qA[index]           = local.q;
        qB[index]           = local.qB;
    }
    gmx_sort_ilist_fe(idef, qA.data(), qB.data());
}

static void gen_local_top(const gmx_mtop_t& mtop,
                          bool              freeEnergyInteractionsAtEnd,
                          bool              bMergeConstr,
                          gmx_localtop_t*   top,
                          fda::FDASettings* ptr_fda_settings)
{
    copyIListsFromMtop(mtop, &top->idef, bMergeConstr, ptr_fda_settings);
    if (freeEnergyInteractionsAtEnd)
    {
        sortFreeEnergyInteractionsAtEnd(mtop, &top->idef);
    }
    top->excls = globalExclusionLists(mtop);
    if (!mtop.intermolecularExclusionGroup.empty())
    {
        addMimicExclusions(&top->excls, mtop.intermolecularExclusionGroup);
    }
}

void gmx_mtop_generate_local_top(const gmx_mtop_t& mtop, gmx_localtop_t* top, bool freeEnergyInteractionsAtEnd,
                                 fda::FDASettings* ptr_fda_settings)
{
    gen_local_top(mtop, freeEnergyInteractionsAtEnd, true, top, ptr_fda_settings);
}

/*! \brief Fills an array with molecule begin/end atom indices
 *
 * \param[in]  mtop   The global topology
 * \param[out] index  Array of size nr. of molecules + 1 to be filled with molecule begin/end indices
 */
static void fillMoleculeIndices(const gmx_mtop_t& mtop, gmx::ArrayRef<int> index)
{
    int globalAtomIndex   = 0;
    int globalMolIndex    = 0;
    index[globalMolIndex] = globalAtomIndex;
    for (const gmx_molblock_t& molb : mtop.molblock)
    {
        int numAtomsPerMolecule = mtop.moltype[molb.type].atoms.nr;
        for (int mol = 0; mol < molb.nmol; mol++)
        {
            globalAtomIndex += numAtomsPerMolecule;
            globalMolIndex += 1;
            index[globalMolIndex] = globalAtomIndex;
        }
    }
}

gmx::RangePartitioning gmx_mtop_molecules(const gmx_mtop_t& mtop)
{
    gmx::RangePartitioning mols;

    for (const gmx_molblock_t& molb : mtop.molblock)
    {
        int numAtomsPerMolecule = mtop.moltype[molb.type].atoms.nr;
        for (int mol = 0; mol < molb.nmol; mol++)
        {
            mols.appendBlock(numAtomsPerMolecule);
        }
    }

    return mols;
}

std::vector<gmx::Range<int>> atomRangeOfEachResidue(const gmx_moltype_t& moltype)
{
    std::vector<gmx::Range<int>> atomRanges;
    int                          currentResidueNumber = moltype.atoms.atom[0].resind;
    int                          startAtom            = 0;
    // Go through all atoms in a molecule to store first and last atoms in each residue.
    for (int i = 0; i < moltype.atoms.nr; i++)
    {
        int residueOfThisAtom = moltype.atoms.atom[i].resind;
        if (residueOfThisAtom != currentResidueNumber)
        {
            // This atom belongs to the next residue, so record the range for the previous residue,
            // remembering that end points to one place past the last atom.
            int endAtom = i;
            atomRanges.emplace_back(startAtom, endAtom);
            // Prepare for the current residue
            startAtom            = endAtom;
            currentResidueNumber = residueOfThisAtom;
        }
    }
    // special treatment for last residue in this molecule.
    atomRanges.emplace_back(startAtom, moltype.atoms.nr);

    return atomRanges;
}

/*! \brief Creates and returns a deprecated t_block struct with molecule indices
 *
 * \param[in] mtop  The global topology
 */
static t_block gmx_mtop_molecules_t_block(const gmx_mtop_t& mtop)
{
    t_block mols;

    mols.nr           = gmx_mtop_num_molecules(mtop);
    mols.nalloc_index = mols.nr + 1;
    snew(mols.index, mols.nalloc_index);

    fillMoleculeIndices(mtop, gmx::arrayRefFromArray(mols.index, mols.nr + 1));

    return mols;
}

static void gen_t_topology(const gmx_mtop_t& mtop, bool bMergeConstr, t_topology* top)
{
    copyAtomtypesFromMtop(mtop, &top->atomtypes);
    for (int ftype = 0; ftype < F_NRE; ftype++)
    {
        top->idef.il[ftype].nr     = 0;
        top->idef.il[ftype].nalloc = 0;
        top->idef.il[ftype].iatoms = nullptr;
    }
    copyFFParametersFromMtop(mtop, &top->idef);
    copyIListsFromMtop(mtop, &top->idef, bMergeConstr, nullptr);

    top->name                        = mtop.name;
    top->atoms                       = gmx_mtop_global_atoms(&mtop);
    top->mols                        = gmx_mtop_molecules_t_block(mtop);
    top->bIntermolecularInteractions = mtop.bIntermolecularInteractions;
    top->symtab                      = mtop.symtab;
}

t_topology gmx_mtop_t_to_t_topology(gmx_mtop_t* mtop, bool freeMTop)
{
    t_topology top;

    gen_t_topology(*mtop, false, &top);

    if (freeMTop)
    {
        // Clear pointers and counts, such that the pointers copied to top
        // keep pointing to valid data after destroying mtop.
        mtop->symtab.symbuf = nullptr;
        mtop->symtab.nr     = 0;
    }
    return top;
}

std::vector<int> get_atom_index(const gmx_mtop_t* mtop)
{

    std::vector<int> atom_index;
    for (const AtomProxy atomP : AtomRange(*mtop))
    {
        const t_atom& local = atomP.atom();
        int           index = atomP.globalAtomNumber();
        if (local.ptype == eptAtom)
        {
            atom_index.push_back(index);
        }
    }
    return atom_index;
}

void convertAtomsToMtop(t_symtab* symtab, char** name, t_atoms* atoms, gmx_mtop_t* mtop)
{
    mtop->symtab = *symtab;

    mtop->name = name;

    mtop->moltype.clear();
    mtop->moltype.resize(1);
    mtop->moltype.back().atoms = *atoms;

    mtop->molblock.resize(1);
    mtop->molblock[0].type = 0;
    mtop->molblock[0].nmol = 1;

    mtop->bIntermolecularInteractions = FALSE;

    mtop->natoms = atoms->nr;

    mtop->haveMoleculeIndices = false;

    mtop->finalize();
}

bool haveFepPerturbedNBInteractions(const gmx_mtop_t& mtop)
{
    for (const gmx_moltype_t& molt : mtop.moltype)
    {
        for (int a = 0; a < molt.atoms.nr; a++)
        {
            if (PERTURBED(molt.atoms.atom[a]))
            {
                return true;
            }
        }
    }

    return false;
}

bool haveFepPerturbedMasses(const gmx_mtop_t& mtop)
{
    for (const gmx_moltype_t& molt : mtop.moltype)
    {
        for (int a = 0; a < molt.atoms.nr; a++)
        {
            const t_atom& atom = molt.atoms.atom[a];
            if (atom.m != atom.mB)
            {
                return true;
            }
        }
    }

    return false;
}

bool haveFepPerturbedMassesInSettles(const gmx_mtop_t& mtop)
{
    for (const gmx_moltype_t& molt : mtop.moltype)
    {
        if (!molt.ilist[F_SETTLE].empty())
        {
            for (int a = 0; a < molt.atoms.nr; a++)
            {
                const t_atom& atom = molt.atoms.atom[a];
                if (atom.m != atom.mB)
                {
                    return true;
                }
            }
        }
    }
    return false;
}

bool havePerturbedConstraints(const gmx_mtop_t& mtop)
{
    // This code assumes that all perturbed constraints parameters are actually used
    const auto& ffparams = mtop.ffparams;

    for (gmx::index i = 0; i < gmx::ssize(ffparams.functype); i++)
    {
        if (ffparams.functype[i] == F_CONSTR || ffparams.functype[i] == F_CONSTRNC)
        {
            const auto& iparams = ffparams.iparams[i];
            if (iparams.constr.dA != iparams.constr.dB)
            {
                return true;
            }
        }
    }

    return false;
}
