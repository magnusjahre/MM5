/*
 * Copyright (c) 2001, 2002, 2003, 2004, 2005
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

#ifndef __ENCUMBERED_CPU_FULL_DYN_INST_HH__
#define __ENCUMBERED_CPU_FULL_DYN_INST_HH__

#include <string>
#include <vector>

#include "base/fast_alloc.hh"
#include "config/full_system.hh"
#include "cpu/exetrace.hh"
#include "cpu/inst_seq.hh"
#include "cpu/static_inst.hh"
#include "encumbered/cpu/full/bpred_update.hh"
#include "encumbered/cpu/full/op_class.hh"
#include "encumbered/cpu/full/spec_memory.hh"
#include "encumbered/cpu/full/spec_state.hh"
#include "encumbered/mem/functional/main.hh"

/**
 * @file
 * Defines a dynamic instruction context.
 */

class FullCPU;

class DynInst : public FastAlloc
{
  public:
    StaticInstPtr<TheISA> staticInst;

    ////////////////////////////////////////////
    //
    // INSTRUCTION EXECUTION
    //
    ////////////////////////////////////////////
    Trace::InstRecord *trace_data;

    void setCPSeq(InstSeqNum seq);

    template <class T>
    Fault read(Addr addr, T &data, unsigned flags);

    template <class T>
    Fault write(T data, Addr addr, unsigned flags,
			uint64_t *res);

    // These functions are only used in CPU models that split
    // effective address computation from the actual memory access.
    void setEA(Addr EA) { panic("FullCPU::setEA() not implemented\n"); }
    Addr getEA() 	{ panic("FullCPU::getEA() not implemented\n"); }

    IntReg *getIntegerRegs(void);
    FunctionalMemory *getMemory(void);

    void prefetch(Addr addr, unsigned flags);
    void writeHint(Addr addr, int size, unsigned flags);
    Fault copySrcTranslate(Addr src);
    Fault copy(Addr dest);

  public:
    bool recover_inst;
    bool btb_missed;
    bool squashed;
    bool serializing_inst;	// flush ROB before dispatching

    short thread_number;
    short spec_mode;
    short asid;		// *data* address space ID, for loads & stores

    FullCPU *cpu;
    SpecExecContext *xc;

    Fault fault;
    Addr eff_addr;	     //!< effective virtual address (lds & stores only)
    Addr phys_eff_addr; //!< effective physical address
    /** Effective virtual address for a copy source. */
    Addr copySrcEffAddr; 
    /** Effective physical address for a copy source. */
    Addr copySrcPhysEffAddr;
    unsigned mem_req_flags;  //!< memory request flags (from translation)


    AnyReg out_oldval1;
    AnyReg out_oldval2;
    AnyReg out_val1;
    AnyReg out_val2;
    int store_size;
    IntReg store_data;

    InstSeqNum fetch_seq;
    InstSeqNum correctPathSeq;

    Addr PC;			/*  PC of this instruction   */
    Addr Next_PC;			/*  Next non-speculative PC  */
    Addr Pred_PC;			/*  Predicted next PC        */
    BPredUpdateRec dir_update;	/* bpred direction update info */

    static int instcount;
    DynInst(StaticInstPtr<TheISA> &_staticInst);
    ~DynInst();

    bool spec_mem_write;	// Did this instruction do a spec write?

    void squash();

    void
    trace_mem(Fault fault,  // last fault
	      MemCmd cmd,          // last command
	      Addr addr,       // virtual address of access
	      void *p,              // memory accessed
	      int nbytes);          // access size

    void dump();
    void dump(std::string &outstring);

    bool pred_taken() {
	return( Pred_PC != (PC + sizeof(MachInst) ) );
    }

    bool mis_predicted() {
	return (Pred_PC != Next_PC);
    }

    //
    //  Instruction types.  Forward checks to StaticInst object.
    //
    bool isNop()	  const { return staticInst->isNop(); }
    bool isMemRef()    	  const { return staticInst->isMemRef(); }
    bool isLoad()	  const { return staticInst->isLoad(); }
    bool isStore()	  const { return staticInst->isStore(); }
    bool isInstPrefetch() const { return staticInst->isInstPrefetch(); }
    bool isDataPrefetch() const { return staticInst->isDataPrefetch(); }
    bool isCopy()         const { return staticInst->isCopy(); }
    bool isInteger()	  const { return staticInst->isInteger(); }
    bool isFloating()	  const { return staticInst->isFloating(); }
    bool isControl()	  const { return staticInst->isControl(); }
    bool isCall()	  const { return staticInst->isCall(); }
    bool isReturn()	  const { return staticInst->isReturn(); }
    bool isDirectCtrl()	  const { return staticInst->isDirectCtrl(); }
    bool isIndirectCtrl() const { return staticInst->isIndirectCtrl(); }
    bool isCondCtrl()	  const { return staticInst->isCondCtrl(); }
    bool isUncondCtrl()	  const { return staticInst->isUncondCtrl(); }
    bool isThreadSync()   const { return staticInst->isThreadSync(); }
    bool isSerializing()  const
	{ return serializing_inst || staticInst->isSerializing(); }
    bool isMemBarrier()   const { return staticInst->isMemBarrier(); }
    bool isWriteBarrier() const { return staticInst->isWriteBarrier(); }
    bool isNonSpeculative() const { return staticInst->isNonSpeculative(); }

    int8_t numSrcRegs()	 const { return staticInst->numSrcRegs(); }
    int8_t numDestRegs() const { return staticInst->numDestRegs(); }

    // the following are used to track physical register usage
    // for machines with separate int & FP reg files
    int8_t numFPDestRegs()  const { return staticInst->numFPDestRegs(); }
    int8_t numIntDestRegs() const { return staticInst->numIntDestRegs(); }

    TheISA::RegIndex destRegIdx(int i) const
    {
	return staticInst->destRegIdx(i);
    }

    TheISA::RegIndex srcRegIdx(int i) const
    {
	return staticInst->srcRegIdx(i);
    }

    OpClass opClass() const { return staticInst->opClass(); }

    bool btb_miss() const { return btb_missed; }

    Addr branchTarget() const { return staticInst->branchTarget(PC); }

    Fault execute()
    {
	return staticInst->execute(this, trace_data);
    }

    // The register accessor methods provide the index of the
    // instruction's operand (e.g., 0 or 1), not the architectural
    // register index, to simplify the implementation of register
    // renaming.  We find the architectural register index by indexing
    // into the instruction's own operand index table.  Note that a
    // raw pointer to the StaticInst is provided instead of a
    // ref-counted StaticInstPtr to redice overhead.  This is fine as
    // long as these methods don't copy the pointer into any long-term
    // storage (which is pretty hard to imagine they would have reason
    // to do).

    uint64_t readIntReg(const StaticInst<TheISA> *si, int idx)
    {
	return xc->readIntReg(si->srcRegIdx(idx));
    }

    float readFloatRegSingle(const StaticInst<TheISA> *si, int idx)
    {
	int reg_idx = si->srcRegIdx(idx) - TheISA::FP_Base_DepTag;
	return xc->readFloatRegSingle(reg_idx);
    }

    double readFloatRegDouble(const StaticInst<TheISA> *si, int idx)
    {
	int reg_idx = si->srcRegIdx(idx) - TheISA::FP_Base_DepTag;
	return xc->readFloatRegDouble(reg_idx);
    }

    uint64_t readFloatRegInt(const StaticInst<TheISA> *si, int idx)
    {
	int reg_idx = si->srcRegIdx(idx) - TheISA::FP_Base_DepTag;
	return xc->readFloatRegInt(reg_idx);
    }

    void setIntReg(const StaticInst<TheISA> *si, int idx, uint64_t val)
    {
	xc->setIntReg(si->destRegIdx(idx), val);
    }

    void setFloatRegSingle(const StaticInst<TheISA> *si, int idx, float val)
    {
	int reg_idx = si->destRegIdx(idx) - TheISA::FP_Base_DepTag;
	xc->setFloatRegSingle(reg_idx, val);
    }

    void setFloatRegDouble(const StaticInst<TheISA> *si, int idx, double val)
    {
	int reg_idx = si->destRegIdx(idx) - TheISA::FP_Base_DepTag;
	xc->setFloatRegDouble(reg_idx, val);
    }

    void setFloatRegInt(const StaticInst<TheISA> *si, int idx, uint64_t val)
    {
	int reg_idx = si->destRegIdx(idx) - TheISA::FP_Base_DepTag;
	xc->setFloatRegInt(reg_idx, val);
    }

    uint64_t readPC() { return xc->readPC(); }
    void setNextPC(uint64_t val) { xc->setNextPC(val); }

    uint64_t readUniq() { return xc->readUniq(); }
    void setUniq(uint64_t val) { xc->setUniq(val); }

    uint64_t readFpcr() { return xc->readFpcr(); }
    void setFpcr(uint64_t val) { xc->setFpcr(val); }

#if FULL_SYSTEM
    uint64_t readIpr(int idx, Fault &fault) { return xc->readIpr(idx, fault); }
    Fault setIpr(int idx, uint64_t val) { return xc->setIpr(idx, val); }
    Fault hwrei() { return xc->hwrei(); }
    int readIntrFlag() { return xc->readIntrFlag(); }
    void setIntrFlag(int val) { xc->setIntrFlag(val); }
    bool inPalMode() { return xc->inPalMode(); }
    void ev5_trap(Fault fault) { xc->ev5_trap(fault); }
    bool simPalCheck(int palFunc) { return xc->simPalCheck(palFunc); }
#else
    void syscall() { xc->syscall(); }
#endif

    bool misspeculating() { return xc->misspeculating(); }
    ExecContext *xcBase() { return xc; }

    void debugPrint(bool write, Addr addr, uint64_t data);
};

template <class T>
inline Fault
DynInst::read(Addr addr, T &data, unsigned flags)
{
    MemReqPtr req = new MemReq(addr, xc, sizeof(T), flags);
    req->asid = asid;

    fault = xc->translateDataReadReq(req);

    // Record key MemReq parameters so we can generate another one
    // just like it for the timing access without calling translate()
    // again (which might mess up the TLB).
    eff_addr = req->vaddr;
    phys_eff_addr = req->paddr;
    mem_req_flags = req->flags;

    /**
     * @todo
     * Replace the disjoint functional memory with a unified one and remove
     * this hack.
     */
#if !FULL_SYSTEM
    req->paddr = req->vaddr;
#endif

    if (fault == No_Fault) {
    	fault = xc->read(req, data);
    	debugPrint(false, addr, (uint64_t) data);
    }
    else {
	// Return a fixed value to keep simulation deterministic even
	// along misspeculated paths.
	data = (T)-1;
    }

    return fault;
}

template <class T>
inline Fault
DynInst::write(T data, Addr addr, unsigned flags, uint64_t *res)
{
    store_size = sizeof(T);
    store_data = data;
    if (spec_mode)
	spec_mem_write = true;

    MemReqPtr req = new MemReq(addr, xc, sizeof(T), flags);

    req->asid = asid;

    fault = xc->translateDataWriteReq(req);

    // Record key MemReq parameters so we can generate another one
    // just like it for the timing access without calling translate()
    // again (which might mess up the TLB).
    eff_addr = req->vaddr;
    phys_eff_addr = req->paddr;
    mem_req_flags = req->flags;

    /**
     * @todo
     * Replace the disjoint functional memory with a unified one and remove
     * this hack.
     */
#if !FULL_SYSTEM
    req->paddr = req->vaddr;
#endif

    if (fault == No_Fault) {
    	fault = xc->write(req, data);
    	debugPrint(true, addr, (uint64_t) data);
    }

    if (res) {
	// always return some result to keep misspeculated paths
	// (which will ignore faults) deterministic
	*res = (fault == No_Fault) ? req->result : 0;
    }

    return fault;
}

#endif // __ENCUMBERED_CPU_FULL_DYN_INST_HH__
