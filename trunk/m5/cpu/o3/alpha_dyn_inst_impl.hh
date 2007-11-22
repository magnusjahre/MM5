/*
 * Copyright (c) 2004, 2005
 * The Regents of The University of Michigan
 * All Rights Reserved
 *
 * This code is part of the M5 simulator, developed by Nathan Binkert,
 * Erik Hallnor, Steve Raasch, and Steve Reinhardt, with contributions
 * from Ron Dreslinski, Dave Greene, Lisa Hsu, Kevin Lim, Ali Saidi, 
 * and Andrew Schultz.
 *
 * Permission is granted to use, copy, create derivative works and
 * redistribute this software and such derivative works for any
 * purpose, so long as the copyright notice above, this grant of
 * permission, and the disclaimer below appear in all copies made; and
 * so long as the name of The University of Michigan is not used in
 * any advertising or publicity pertaining to the use or distribution
 * of this software without specific, written prior authorization.
 *
 * THIS SOFTWARE IS PROVIDED AS IS, WITHOUT REPRESENTATION FROM THE
 * UNIVERSITY OF MICHIGAN AS TO ITS FITNESS FOR ANY PURPOSE, AND
 * WITHOUT WARRANTY BY THE UNIVERSITY OF MICHIGAN OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE. THE REGENTS OF THE UNIVERSITY OF MICHIGAN SHALL NOT BE
 * LIABLE FOR ANY DAMAGES, INCLUDING DIRECT, SPECIAL, INDIRECT,
 * INCIDENTAL, OR CONSEQUENTIAL DAMAGES, WITH RESPECT TO ANY CLAIM
 * ARISING OUT OF OR IN CONNECTION WITH THE USE OF THE SOFTWARE, EVEN
 * IF IT HAS BEEN OR IS HEREAFTER ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGES.
 */

#include "cpu/o3/alpha_dyn_inst.hh"

template <class Impl>
AlphaDynInst<Impl>::AlphaDynInst(MachInst inst, Addr PC, Addr Pred_PC, 
                                 InstSeqNum seq_num, FullCPU *cpu)
    : BaseDynInst<Impl>(inst, PC, Pred_PC, seq_num, cpu)
{
    // Make sure to have the renamed register entries set to the same
    // as the normal register entries.  It will allow the IQ to work
    // without any modifications.
    for (int i = 0; i < this->staticInst->numDestRegs(); i++)
    {
        _destRegIdx[i] = this->staticInst->destRegIdx(i);
    }

    for (int i = 0; i < this->staticInst->numSrcRegs(); i++)
    {
        _srcRegIdx[i] = this->staticInst->srcRegIdx(i);
        this->_readySrcRegIdx[i] = 0;
    }

}

template <class Impl>
AlphaDynInst<Impl>::AlphaDynInst(StaticInstPtr<AlphaISA> &_staticInst)
    : BaseDynInst<Impl>(_staticInst)
{
    // Make sure to have the renamed register entries set to the same
    // as the normal register entries.  It will allow the IQ to work
    // without any modifications.
    for (int i = 0; i < _staticInst->numDestRegs(); i++)
    {
        _destRegIdx[i] = _staticInst->destRegIdx(i);
    }

    for (int i = 0; i < _staticInst->numSrcRegs(); i++)
    {
        _srcRegIdx[i] = _staticInst->srcRegIdx(i);
    }
}

template <class Impl>
uint64_t 
AlphaDynInst<Impl>::readUniq() 
{ 
    return this->cpu->readUniq(); 
}

template <class Impl>
void 
AlphaDynInst<Impl>::setUniq(uint64_t val)
{ 
    this->cpu->setUniq(val); 
}

template <class Impl>
uint64_t 
AlphaDynInst<Impl>::readFpcr() 
{
    return this->cpu->readFpcr(); 
}

template <class Impl>
void 
AlphaDynInst<Impl>::setFpcr(uint64_t val) 
{ 
    this->cpu->setFpcr(val); 
}

#if FULL_SYSTEM
template <class Impl>
uint64_t
AlphaDynInst<Impl>::readIpr(int idx, Fault &fault)
{ 
    return this->cpu->readIpr(idx, fault); 
}

template <class Impl>
Fault 
AlphaDynInst<Impl>::setIpr(int idx, uint64_t val)
{ 
    return this->cpu->setIpr(idx, val); 
}

template <class Impl>
Fault 
AlphaDynInst<Impl>::hwrei() 
{ 
    return this->cpu->hwrei(); 
}

template <class Impl>
int
AlphaDynInst<Impl>::readIntrFlag() 
{ 
return this->cpu->readIntrFlag(); 
}

template <class Impl>
void 
AlphaDynInst<Impl>::setIntrFlag(int val) 
{ 
    this->cpu->setIntrFlag(val);
}

template <class Impl>
bool 
AlphaDynInst<Impl>::inPalMode()
{ 
    return this->cpu->inPalMode();
}

template <class Impl>
void 
AlphaDynInst<Impl>::trap(Fault fault)
{ 
    this->cpu->trap(fault); 
}

template <class Impl>
bool 
AlphaDynInst<Impl>::simPalCheck(int palFunc)
{ 
    return this->cpu->simPalCheck(palFunc); 
}
#else
template <class Impl>
void 
AlphaDynInst<Impl>::syscall() 
{ 
    this->cpu->syscall(this->threadNumber); 
}
#endif

