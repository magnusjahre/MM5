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

#ifndef __CPU_O3_CPU_LDSTQ_HH__
#define __CPU_O3_CPU_LDSTQ_HH__

#include <map>
#include <queue>

#include "config/full_system.hh"
#include "base/hashmap.hh"
#include "cpu/inst_seq.hh"
#include "mem/mem_interface.hh"
#include "sim/sim_object.hh"

template <class Impl>
class LDSTQ {
  public:
    typedef typename Impl::Params Params;
    typedef typename Impl::FullCPU FullCPU;
    typedef typename Impl::DynInstPtr DynInstPtr;
    typedef typename Impl::CPUPol::IEW IEW;

    typedef typename std::map<InstSeqNum, DynInstPtr>::iterator ld_map_it_t;

    LDSTQ(Params &params);
    
    void setCPU(FullCPU *cpu_ptr) 
    { cpu = cpu_ptr; }

    void setIEW(IEW *iew_ptr) 
    { iewStage = iew_ptr; }

    void tick() { usedPorts = 0; }

    void insert(DynInstPtr &inst);
    void insertLoad(DynInstPtr &load_inst);
    void insertStore(DynInstPtr &store_inst);
    Fault executeLoad(DynInstPtr &inst);
    Fault executeStore(DynInstPtr &inst);

    void commitLoad();
    void commitLoad(InstSeqNum &inst);
    void commitLoads(InstSeqNum &youngest_inst);

    void commitStores(InstSeqNum &youngest_inst);

    void writebackStores();

    void squash(const InstSeqNum &squashed_num);

    bool violation();

    DynInstPtr getMemDepViolator();

    int numLoadsReady();

    int numLoads() { return loads; }

    int numStores() { return stores; }

    bool lqFull() { return loads >= (LQEntries - 1); }

    bool sqFull() { return stores >= (SQEntries - 1); }

    void dumpInsts();

#if !FULL_SYSTEM
    void writebackAllInsts();
#endif

 private:
    FullCPU *cpu;

    IEW *iewStage;

    MemInterface *dcacheInterface;

  private:
    struct SQEntry {
        SQEntry()
            : inst(NULL), req(NULL), size(0), data(0), canWB(0), committed(0)
        { }

        SQEntry(DynInstPtr &_inst)
            : inst(_inst), req(NULL), size(0), data(0), canWB(0), committed(0)
        { }

        DynInstPtr inst;
        MemReqPtr req;
        int size;
        IntReg data;
        bool canWB;
        bool committed;
    };

    std::vector<SQEntry> storeQueue;

    std::vector<DynInstPtr> loadQueue;

    // Consider making these 16 bits
    unsigned LQEntries;
    unsigned SQEntries;

    int loads;
    int stores;
    int storesToWB;

    int loadHead;
    int loadTail;

    int storeHead;
    int storeTail;
    
    /// @todo Consider moving to a more advanced model with write vs read ports
    int cachePorts;

    int usedPorts;

    DynInstPtr loadFaultInst;
    DynInstPtr storeFaultInst;

    DynInstPtr memDepViolator;

    // Will also need how many read/write ports the Dcache has.  Or keep track
    // of that in stage that is one level up, and only call executeLoad/Store
    // the appropriate number of times.

  public:
    template <class T>
    Fault read(MemReqPtr &req, T &data, int load_idx);

    template <class T>
    Fault write(MemReqPtr &req, T &data, int store_idx);
};

// Should I pass in the store index as well?  
template <class Impl>
template <class T>
Fault 
LDSTQ<Impl>::read(MemReqPtr &req, T &data, int load_idx)
{
    assert(loadQueue[load_idx]);

    // translate to physical address
    Fault fault = cpu->translateDataReadReq(req);

    // Make sure this isn't an uncacheable access
    // A bit of a hackish way to get uncached accesses to work only if they're
    // at the head of the LSQ and are ready to commit (at the head of the ROB
    // too).
    if (req->flags & UNCACHEABLE && 
        (load_idx != loadHead || !loadQueue[loadHead]->readyToCommit())) {
//        if (!dcacheInterface)
//            recordEvent("Uncached Read");

        return Machine_Check_Fault;
    }

    // Check the SQ for any previous stores that might lead to forwarding
    int store_idx = loadQueue[load_idx]->sqIdx;

    int size = 0;

    DPRINTF(LDSTQ, "LDSTQ: Read called, load idx: %i, store idx: %i, addr:"
            " %#x\n", load_idx, store_idx, req->paddr);
    
    if (store_idx != -1) {
        while (1) {
            if (store_idx == storeHead) {
                break;
            }

            store_idx = store_idx - 1;
            if (store_idx < 0) {
                store_idx += SQEntries;
            }

            assert(storeQueue[store_idx].inst);

            size = storeQueue[store_idx].size;
            if (size == 0)
                continue;

            if ((storeQueue[store_idx].inst->effAddr & ~(size - 1)) == 
                (req->paddr & ~(size - 1))) {
                int shift_amt = req->paddr & (size - 1);
                // Assumes byte addressing
                shift_amt = shift_amt << 3;

                // Cast this to type T?
                data = storeQueue[store_idx].data >> shift_amt;

                DPRINTF(LDSTQ, "LDSTQ: Forwarding from store idx %i to load to "
                        "addr %#x, data %#x\n", store_idx, req->paddr, data);

                // Should keep track of stat for forwarded data
                return No_Fault;
            }

        }
    }


    // If there's no forwarding case, then go access memory
    // May be worthwhile having memory object on its own?
    // Would have to replicate read function
    if (fault == No_Fault) {
        DPRINTF(LDSTQ, "LDSTQ: Doing functional access for inst PC %#x\n",
                loadQueue[load_idx]->readPC());
        fault = cpu->read(req, data);

        ++usedPorts;
    }

    // if we have a cache, do cache access too
    if (fault == No_Fault && dcacheInterface) {
        DPRINTF(LDSTQ, "LDSTQ: Doing timing access for inst PC %#x\n",
                loadQueue[load_idx]->readPC());
	req->cmd = Read;
	req->completionEvent = NULL;
        req->expectCompletionEvent = false;
	req->time = curTick;
	MemAccessResult result = dcacheInterface->access(req);

	// Ugly hack to get an event scheduled *only* if the access is
	// a miss.  We really should add first-class support for this
	// at some point.
	if (result != MA_HIT && dcacheInterface->doEvents()) {
	    req->completionEvent = new typename
                IEW::WritebackEvent(loadQueue[load_idx], iewStage);
            req->expectCompletionEvent = true;
	} 
    }

    return fault;
}

template <class Impl>
template <class T>
Fault 
LDSTQ<Impl>::write(MemReqPtr &req, T &data, int store_idx)
{

    assert(storeQueue[store_idx].inst);

    DPRINTF(LDSTQ, "LDSTQ: Doing write to store idx %i, addr %#x data %#x\n",
            store_idx, req->paddr, data);

    storeQueue[store_idx].req = req;
    storeQueue[store_idx].size = sizeof(T);
    storeQueue[store_idx].data = data;

    if (store_idx == storeHead && storeQueue[store_idx].inst->readyToCommit()) {
        return No_Fault;
    } else {
        return Fake_Mem_Fault;
    }
}


#endif // __CPU_O3_CPU_LDSTQ_HH__
