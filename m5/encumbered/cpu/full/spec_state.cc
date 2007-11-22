/*
 * Copyright (c) 2000, 2001, 2002, 2003, 2004, 2005
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

#include <cstdio>
#include <string>

#include "encumbered/cpu/full/fetch.hh"		/* for PC[] */
#include "encumbered/cpu/full/spec_memory.hh"
#include "encumbered/cpu/full/spec_state.hh"


#if FULL_SYSTEM
SpecExecContext::SpecExecContext(BaseCPU *_cpu, int _thread_num,
				 System *_system,
				 AlphaITB *_itb, AlphaDTB *_dtb,
				 FunctionalMemory *_mem)
    : ExecContext(_cpu, _thread_num, _system, _itb, _dtb, _mem)
#else
SpecExecContext::SpecExecContext(BaseCPU *_cpu,  int _thread_num,
				 Process *_process, int _asid)
    : ExecContext(_cpu, _thread_num, _process, _asid)
#endif
{
    // initially in non-speculative mode
    spec_mode = 0;

    // allocate speculative memory layer
    spec_mem = new SpeculativeMemory("bad_mem", mem);

    /* register state is from non-speculative state buffers */
    use_spec_R.reset();
    use_spec_F.reset();
    use_spec_C.reset();
}


void
SpecExecContext::reset_spec_state()
{
    /* reset copied-on-write register bitmasks back to non-speculative state */
    use_spec_R.reset();
    use_spec_F.reset();
    use_spec_C.reset();
}


void
SpecExecContext::takeOverFrom(ExecContext *oldContext)
{
    ExecContext::takeOverFrom(oldContext);
    reset_spec_state();

    // we only copy over the non-speculative state, so by definition
    // we're not misspeculating at this point
    spec_mode = 0;
}
