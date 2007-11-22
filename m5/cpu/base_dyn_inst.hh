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

#ifndef __CPU_BASE_DYN_INST_HH__
#define __CPU_BASE_DYN_INST_HH__

#include <string>
#include <vector>

#include "base/fast_alloc.hh"
#include "base/trace.hh"
#include "config/full_system.hh"
#include "cpu/exetrace.hh"
#include "cpu/inst_seq.hh"
#include "cpu/o3/comm.hh"
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

// Forward declaration.
template <class ISA>
class StaticInstPtr;

template <class Impl>
class BaseDynInst : public FastAlloc, public RefCounted
{
  public:
    // Typedef for the CPU.
    typedef typename Impl::FullCPU FullCPU;

    //Typedef to get the ISA.
    typedef typename Impl::ISA ISA;

    /// Binary machine instruction type.
    typedef typename ISA::MachInst MachInst;
    /// Memory address type.
    typedef typename ISA::Addr	   Addr;
    /// Logical register index type.
    typedef typename ISA::RegIndex RegIndex;
    /// Integer register index type.
    typedef typename ISA::IntReg   IntReg;

    enum {
	MaxInstSrcRegs = ISA::MaxInstSrcRegs,	//< Max source regs
	MaxInstDestRegs = ISA::MaxInstDestRegs,	//< Max dest regs
    };

    /** The static inst used by this dyn inst. */
    StaticInstPtr<ISA> staticInst;

    ////////////////////////////////////////////
    //
    // INSTRUCTION EXECUTION
    //
    ////////////////////////////////////////////
    Trace::InstRecord *traceData;

    template <class T>
    Fault read(Addr addr, T &data, unsigned flags);

    template <class T>
    Fault write(T data, Addr addr, unsigned flags,
			uint64_t *res);

    void prefetch(Addr addr, unsigned flags);
    void writeHint(Addr addr, int size, unsigned flags);
    Fault copySrcTranslate(Addr src);
    Fault copy(Addr dest);

    /** @todo: Consider making this private. */
  public:
    /** Is this instruction valid. */
    bool valid;

    /** The sequence number of the instruction. */
    InstSeqNum seqNum;

    /** How many source registers are ready. */
    unsigned readyRegs;

    /** Is the instruction completed. */
    bool completed;

    /** Can this instruction issue. */
    bool canIssue;

    /** Has this instruction issued. */
    bool issued;

    /** Has this instruction executed (or made it through execute) yet. */
    bool executed;
    
    /** Can this instruction commit. */
    bool canCommit;

    /** Is this instruction squashed. */
    bool squashed;

    /** Is this instruction squashed in the instruction queue. */
    bool squashedInIQ;

    /** Is this a recover instruction. */
    bool recoverInst;

    /** Is this a thread blocking instruction. */
    bool blockingInst;	/* this inst has called thread_block() */

    /** Is this a thread syncrhonization instruction. */
    bool threadsyncWait;

    /** The thread this instruction is from. */
    short threadNumber;

    /** data address space ID, for loads & stores. */
    short asid;

    /** Pointer to the FullCPU object. */
    FullCPU *cpu;

    /** Pointer to the exec context.  Will not exist in the final version. */
    ExecContext *xc;

    /** The kind of fault this instruction has generated. */
    Fault fault;

    /** The effective virtual address (lds & stores only). */
    Addr effAddr;

    /** The effective physical address. */
    Addr physEffAddr;

    /** Effective virtual address for a copy source. */
    Addr copySrcEffAddr;

    /** Effective physical address for a copy source. */
    Addr copySrcPhysEffAddr;

    /** The memory request flags (from translation). */
    unsigned memReqFlags;

    /** The size of the data to be stored. */
    int storeSize;

    /** The data to be stored. */
    IntReg storeData;

    union Result {
        uint64_t integer;
        float fp;
        double dbl;
    };

    /** The result of the instruction; assumes for now that there's only one
     *  destination register.
     */
    Result instResult;

    /** PC of this instruction. */
    Addr PC;

    /** Next non-speculative PC.  It is not filled in at fetch, but rather
     *  once the target of the branch is truly known (either decode or 
     *  execute).
     */
    Addr nextPC;

    /** Predicted next PC. */
    Addr predPC;

    /** Count of total number of dynamic instructions. */
    static int instcount;
    
    /** Whether or not the source register is ready.  Not sure this should be
     *  here vs. the derived class.
     */
    bool _readySrcRegIdx[MaxInstSrcRegs];

  public:
    /** BaseDynInst constructor given a binary instruction. */
    BaseDynInst(MachInst inst, Addr PC, Addr Pred_PC, InstSeqNum seq_num,
                FullCPU *cpu);

    /** BaseDynInst constructor given a static inst pointer. */
    BaseDynInst(StaticInstPtr<ISA> &_staticInst);

    /** BaseDynInst destructor. */
    ~BaseDynInst();

  private:
    /** Function to initialize variables in the constructors. */
    void initVars();

  public:
    void
    trace_mem(Fault fault,      // last fault
	      MemCmd cmd,       // last command
	      Addr addr,        // virtual address of access
	      void *p,          // memory accessed
	      int nbytes);      // access size

    /** Dumps out contents of this BaseDynInst. */
    void dump();

    /** Dumps out contents of this BaseDynInst into given string. */
    void dump(std::string &outstring);

    /** Returns the fault type. */
    Fault getFault() { return fault; }    

    /** Checks whether or not this instruction has had its branch target
     *  calculated yet.  For now it is not utilized and is hacked to be
     *  always false.
     */
    bool doneTargCalc() { return false; }
   
    /** Returns the next PC.  This could be the speculative next PC if it is
     *  called prior to the actual branch target being calculated.
     */
    Addr readNextPC() { return nextPC; }

    /** Set the predicted target of this current instruction. */
    void setPredTarg(Addr predicted_PC) { predPC = predicted_PC; }

    /** Returns the predicted target of the branch. */
    Addr readPredTarg() { return predPC; }

    /** Returns whether the instruction was predicted taken or not. */
    bool predTaken() {
        return( predPC != (PC + sizeof(MachInst) ) );
    }

    /** Returns whether the instruction mispredicted. */
    bool mispredicted() { return (predPC != nextPC); }

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
    bool isSerializing()  const { return staticInst->isSerializing(); }
    bool isMemBarrier()   const { return staticInst->isMemBarrier(); }
    bool isWriteBarrier() const { return staticInst->isWriteBarrier(); }
    bool isNonSpeculative() const { return staticInst->isNonSpeculative(); }

    /** Returns the opclass of this instruction. */
    OpClass opClass() const { return staticInst->opClass(); }

    /** Returns the branch target address. */
    Addr branchTarget() const { return staticInst->branchTarget(PC); }

    /** Number of source registers. */
    int8_t numSrcRegs()	 const { return staticInst->numSrcRegs(); }

    /** Number of destination registers. */
    int8_t numDestRegs() const { return staticInst->numDestRegs(); }

    // the following are used to track physical register usage
    // for machines with separate int & FP reg files
    int8_t numFPDestRegs()  const { return staticInst->numFPDestRegs(); }
    int8_t numIntDestRegs() const { return staticInst->numIntDestRegs(); }

    /** Returns the logical register index of the i'th destination register. */
    RegIndex destRegIdx(int i) const
    {
        return staticInst->destRegIdx(i);
    }

    /** Returns the logical register index of the i'th source register. */
    RegIndex srcRegIdx(int i) const
    {
        return staticInst->srcRegIdx(i);
    }

    /** Returns the result of an integer instruction. */
    uint64_t readIntResult() { return instResult.integer; }

    /** Returns the result of a floating point instruction. */
    float readFloatResult() { return instResult.fp; }

    /** Returns the result of a floating point (double) instruction. */
    double readDoubleResult() { return instResult.dbl; }

    //Push to .cc file.
    /** Records that one of the source registers is ready. */
    void markSrcRegReady()
    {
        ++readyRegs;
        if(readyRegs == numSrcRegs()) {
            canIssue = true;
        }
    }
    
    /** Marks a specific register as ready.
     *  @todo: Move this to .cc file.
     */
    void markSrcRegReady(RegIndex src_idx)
    {
        ++readyRegs;
        
        _readySrcRegIdx[src_idx] = 1;
        
        if(readyRegs == numSrcRegs()) {
            canIssue = true;
        }
    }

    /** Returns if a source register is ready. */
    bool isReadySrcRegIdx(int idx) const
    {
        return this->_readySrcRegIdx[idx];
    }

    /** Sets this instruction as completed. */
    void setCompleted() { completed = true; }

    /** Returns whethe or not this instruction is completed. */
    bool isCompleted() const { return completed; }

    /** Sets this instruction as ready to issue. */
    void setCanIssue() { canIssue = true; }

    /** Returns whether or not this instruction is ready to issue. */
    bool readyToIssue() const { return canIssue; }

    /** Sets this instruction as issued from the IQ. */
    void setIssued() { issued = true; }

    /** Returns whether or not this instruction has issued. */
    bool isIssued() const { return issued; }

    /** Sets this instruction as executed. */
    void setExecuted() { executed = true; }

    /** Returns whether or not this instruction has executed. */
    bool isExecuted() const { return executed; }

    /** Sets this instruction as ready to commit. */
    void setCanCommit() { canCommit = true; }

    /** Clears this instruction as being ready to commit. */
    void clearCanCommit() { canCommit = false; }

    /** Returns whether or not this instruction is ready to commit. */
    bool readyToCommit() const { return canCommit; }

    /** Sets this instruction as squashed. */
    void setSquashed() { squashed = true; }

    /** Returns whether or not this instruction is squashed. */
    bool isSquashed() const { return squashed; }

    /** Sets this instruction as squashed in the IQ. */
    void setSquashedInIQ() { squashedInIQ = true; }

    /** Returns whether or not this instruction is squashed in the IQ. */
    bool isSquashedInIQ() const { return squashedInIQ; }

    /** Read the PC of this instruction. */
    const Addr readPC() const { return PC; }

    /** Set the next PC of this instruction (its actual target). */
    void setNextPC(uint64_t val) { nextPC = val; }

    /** Returns the exec context.
     *  @todo: Remove this once the ExecContext is no longer used.
     */
    ExecContext *xcBase() { return xc; }

  private:
    /** Instruction effective address.
     *  @todo: Consider if this is necessary or not.
     */
    Addr instEffAddr;
    /** Whether or not the effective address calculation is completed.
     *  @todo: Consider if this is necessary or not.
     */
    bool eaCalcDone;

  public:
    /** Sets the effective address. */
    void setEA(Addr &ea) { instEffAddr = ea; eaCalcDone = true; }

    /** Returns the effective address. */
    const Addr &getEA() const { return instEffAddr; }

    /** Returns whether or not the eff. addr. calculation has been completed. */
    bool doneEACalc() { return eaCalcDone; }

    /** Returns whether or not the eff. addr. source registers are ready. */
    bool eaSrcsReady();

  public:
    /** Load queue index. */
    int16_t lqIdx;

    /** Store queue index. */
    int16_t sqIdx;
};

template<class Impl>
template<class T>
inline Fault
BaseDynInst<Impl>::read(Addr addr, T &data, unsigned flags)
{
    MemReqPtr req = new MemReq(addr, xc, sizeof(T), flags);
    req->asid = asid;

    fault = cpu->translateDataReadReq(req);

    // Record key MemReq parameters so we can generate another one
    // just like it for the timing access without calling translate()
    // again (which might mess up the TLB).
    // Do I ever really need this? -KTL 3/05
    effAddr = req->vaddr;
    physEffAddr = req->paddr;
    memReqFlags = req->flags;

    /**
     * @todo
     * Replace the disjoint functional memory with a unified one and remove
     * this hack.
     */
#if !FULL_SYSTEM
    req->paddr = req->vaddr;
#endif

    if (fault == No_Fault) {
	fault = cpu->read(req, data, lqIdx);
    } else {
	// Return a fixed value to keep simulation deterministic even
	// along misspeculated paths.
	data = (T)-1;
    }

    if (traceData) {
	traceData->setAddr(addr);
	traceData->setData(data);
    }

    return fault;
}

template<class Impl>
template<class T>
inline Fault
BaseDynInst<Impl>::write(T data, Addr addr, unsigned flags, uint64_t *res)
{
    if (traceData) {
	traceData->setAddr(addr);
	traceData->setData(data);
    }

    MemReqPtr req = new MemReq(addr, xc, sizeof(T), flags);

    req->asid = asid;

    fault = cpu->translateDataWriteReq(req);

    // Record key MemReq parameters so we can generate another one
    // just like it for the timing access without calling translate()
    // again (which might mess up the TLB).
    effAddr = req->vaddr;
    physEffAddr = req->paddr;
    memReqFlags = req->flags;

    /**
     * @todo
     * Replace the disjoint functional memory with a unified one and remove
     * this hack.
     */
#if !FULL_SYSTEM
    req->paddr = req->vaddr;
#endif

    if (fault == No_Fault) {
	fault = cpu->write(req, data, sqIdx);
    }

    if (res) {
	// always return some result to keep misspeculated paths
	// (which will ignore faults) deterministic
	*res = (fault == No_Fault) ? req->result : 0;
    }

    return fault;
}

#endif // __CPU_BASE_DYN_INST_HH__
