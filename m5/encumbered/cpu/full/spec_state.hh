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

/*
 * This file is a part of the SimpleScalar tool suite written by
 * Todd M. Austin as a part of the Multiscalar Research Project.
 *
 * The tool suite is currently maintained by Doug Burger and Todd M. Austin.
 *
 * Copyright (C) 1997, 1998 by Todd M. Austin
 *
 * This source file is distributed "as is" in the hope that it will be
 * useful.  The tool set comes with no warranty, and no author or
 * distributor accepts any responsibility for the consequences of its
 * use.
 *
 * Everyone is granted permission to copy, modify and redistribute
 * this tool set under the following conditions:
 *
 *    This source code is distributed for non-commercial use only.
 *    Please contact the maintainer for restrictions applying to
 *    commercial use.
 *
 *    Permission is granted to anyone to make or distribute copies
 *    of this source code, either as received or modified, in any
 *    medium, provided that all copyright notices, permission and
 *    nonwarranty notices are preserved, and that the distributor
 *    grants the recipient permission for further redistribution as
 *    permitted by this document.
 *
 *    Permission is granted to distribute this file in compiled
 *    or executable form under the same conditions that apply for
 *    source code, provided that either:
 *
 *    A. it is accompanied by the corresponding machine-readable
 *       source code,
 *    B. it is accompanied by a written offer, with no time limit,
 *       to give anyone a machine-readable copy of the corresponding
 *       source code in return for reimbursement of the cost of
 *       distribution.  This written offer must permit verbatim
 *       duplication by anyone, or
 *    C. it is distributed by someone who received only the
 *       executable form, and is accompanied by a copy of the
 *       written offer of source code that they received concurrently.
 *
 * In other words, you are welcome to use, share and improve this
 * source file.  You are forbidden to forbid anyone else to use, share
 * and improve what you give them.
 */

#ifndef __ENCUMBERED_CPU_FULL_SPEC_STATE_HH__
#define __ENCUMBERED_CPU_FULL_SPEC_STATE_HH__

#include <deque>

#include "config/full_system.hh"
#include "cpu/exec_context.hh"
#include "cpu/smt.hh"
#include "encumbered/cpu/full/create_vector.hh"
#include "encumbered/cpu/full/spec_memory.hh"

class Process;

//
// The SpecExecContext object extends the basic execution context
// to support speculative execution down both correct and incorrect
// paths.
//
struct SpecExecContext : ExecContext
{
    int spec_mode;	// non-zero: context is currently misspeculating

    SpeculativeMemory *spec_mem;	// reflects wrong-path writes

    /* speculative integer register file */
    std::bitset<NumIntRegs> use_spec_R;
    IntRegFile specIntRegFile;

    /* speculative floating point register file */
    std::bitset<NumFloatRegs> use_spec_F;
    FloatRegFile specFloatRegFile;

    /* speculative miscellaneous registers */
    std::bitset<NumMiscRegs> use_spec_C;
    MiscRegFile specMiscRegs;

#if FULL_SYSTEM
    SpecExecContext(BaseCPU *_cpu, int _thread_num, System *_system,
		    AlphaITB *_itb, AlphaDTB *_dtb, FunctionalMemory *_dmem);
#else
    // constructor: initialize from given process
    SpecExecContext(BaseCPU *, int _thread_num, Process *, int _asid);
#endif

    virtual ~SpecExecContext() {};

    /*
     * Reset register maps and speculative memory state for given thread
     */
    void reset_spec_state();

    virtual void takeOverFrom(ExecContext *oldContext);

    template <class T>
    Fault read(MemReqPtr &req, T &data)
    {
	if (!spec_mode) {
		DPRINTF(FuncMem, "Not speculating, redirecting request for addr 0x%x to functional memory\n", req->paddr);
	    return ExecContext::read(req, data);
	} else {
	    if (req->flags & (UNCACHEABLE | LOCKED)) {
		// don't speculatively issue uncacheable or locked accesses.
		// caller will look at data, so force a return value
		// here so that simulation is deterministic
		data = 0;
		return No_Fault;
	    } else {
	    	DPRINTF(FuncMem, "Speculating, redirecting request for addr 0x%x to speculative memory\n", req->paddr);
		Fault fault = spec_mem->read(req, data);

		if (fault != No_Fault) {
		    // Faults will be ignored on misspeculated accesses,
		    // so again we need to force a return value here so
		    // that simulation is deterministic
		    data = 0;
		}

		return fault;
	    }
	}
    }

    template <class T>
    Fault write(MemReqPtr &req, T &data)
    {
	if (!spec_mode) {
	    return ExecContext::write(req, data);
	} else {
	    // Wrong-path uncacheable writes can turn in to
	    // uncacheable reads past the speculative memory layer
	    // (since the spec mem layer is "write allocate"), so
	    // don't do them.  Filter out LOCKED accesses here too
	    // (LOCKED flag set), since spec_mem->write() won't
	    // set the result flag.
	    if (req->flags & (UNCACHEABLE | LOCKED)) {
		req->result = 0; // set this in case it's LOCKED
		return No_Fault;
	    }
	    return spec_mem->write(req, data);
	}
    }


    virtual bool misspeculating()
    {
	return (spec_mode != 0);
    }
    //
    // New accessors for new decoder.
    //
    uint64_t readIntReg(int reg_idx)
    {
	return (use_spec_R[reg_idx]
		? specIntRegFile[reg_idx]
		: regs.intRegFile[reg_idx]);
    }

    float readFloatRegSingle(int reg_idx)
    {
	return (use_spec_F[reg_idx]
		? (float)specFloatRegFile.d[reg_idx]
		: (float)regs.floatRegFile.d[reg_idx]);
    }

    double readFloatRegDouble(int reg_idx)
    {
	return (use_spec_F[reg_idx]
		? specFloatRegFile.d[reg_idx]
		: regs.floatRegFile.d[reg_idx]);
    }

    uint64_t readFloatRegInt(int reg_idx)
    {
	return (use_spec_F[reg_idx]
		? specFloatRegFile.q[reg_idx]
		: regs.floatRegFile.q[reg_idx]);
    }

    void setIntReg(int reg_idx, uint64_t val)
    {
	if (spec_mode) {
	    specIntRegFile[reg_idx] = val;
	    use_spec_R[reg_idx] = true;
	}
	else {
	    regs.intRegFile[reg_idx] = val;
	}
    }

    void setFloatRegSingle(int reg_idx, float val)
    {
	if (spec_mode) {
	    specFloatRegFile.d[reg_idx] = val;
	    use_spec_F[reg_idx] = true;
	}
	else {
	    regs.floatRegFile.d[reg_idx] = val;
	}
    }

    void setFloatRegDouble(int reg_idx, double val)
    {
	if (spec_mode) {
	    specFloatRegFile.d[reg_idx] = val;
	    use_spec_F[reg_idx] = true;
	}
	else {
	    regs.floatRegFile.d[reg_idx] = val;
	}
    }

    void setFloatRegInt(int reg_idx, uint64_t val)
    {
	if (spec_mode) {
	    specFloatRegFile.q[reg_idx] = val;
	    use_spec_F[reg_idx] = true;
	}
	else {
	    regs.floatRegFile.q[reg_idx] = val;
	}
    }

    uint64_t readPC()
    {
	return regs.pc;
    }

    void setNextPC(uint64_t val)
    {
	regs.npc = val;
    }

    uint64_t readFpcr()
    {
	int idx = AlphaISA::Fpcr_DepTag - AlphaISA::Ctrl_Base_DepTag;
	return (use_spec_C[idx] ? specMiscRegs.fpcr : regs.miscRegs.fpcr);
    }

    void setFpcr(uint64_t val)
    {
	int idx = AlphaISA::Fpcr_DepTag - AlphaISA::Ctrl_Base_DepTag;
	if (spec_mode) {
	    specMiscRegs.fpcr = val;
	    use_spec_C[idx] = true;
	}
	else {
	    regs.miscRegs.fpcr = val;
	}
    }

    uint64_t readUniq()
    {
	int idx = AlphaISA::Uniq_DepTag - AlphaISA::Ctrl_Base_DepTag;
	return (use_spec_C[idx] ? specMiscRegs.uniq : regs.miscRegs.uniq);
    }

    void setUniq(uint64_t val)
    {
	int idx = AlphaISA::Uniq_DepTag - AlphaISA::Ctrl_Base_DepTag;
	if (spec_mode) {
	    specMiscRegs.uniq = val;
	    use_spec_C[idx] = true;
	}
	else {
	    regs.miscRegs.uniq = val;
	}
    }

#if !FULL_SYSTEM
    void syscall()
    {
	if (!spec_mode)
	    process->syscall(this);
    }
#endif
};

#endif // __ENCUMBERED_CPU_FULL_SPEC_STATE_HH__
