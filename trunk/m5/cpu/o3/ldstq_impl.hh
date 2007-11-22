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

#include "cpu/o3/ldstq.hh"

template <class Impl>
LDSTQ<Impl>::LDSTQ(Params &params)
    : dcacheInterface(params.dcacheInterface), 
      LQEntries(params.LQEntries + 1), SQEntries(params.SQEntries + 1),
      loads(0), stores(0), storesToWB(0)
{
    DPRINTF(LDSTQ, "LDSTQ: Creating LDSTQ object.\n");

    storeQueue.resize(SQEntries);
    loadQueue.resize(LQEntries);

    // May want to initialize these entries to NULL

    loadHead = loadTail = 0;

    storeHead = storeTail = 0;

    usedPorts = 0;
    cachePorts = 200;  // Hard code for now!
}

template <class Impl>
void
LDSTQ<Impl>::insert(DynInstPtr &inst)
{
    // Make sure we really have a memory reference.
    assert(inst->isMemRef());

    // Make sure it's one of the two classes of memory references.
    assert(inst->isLoad() || inst->isStore());

    if (inst->isLoad()) {
        insertLoad(inst);
    } else {
        insertStore(inst);
    }
}

template <class Impl>
void
LDSTQ<Impl>::insertLoad(DynInstPtr &load_inst)
{
    assert((loadTail + 1) % LQEntries != loadHead && loads < LQEntries);

    DPRINTF(LDSTQ, "LDSTQ: Inserting load PC %#x, idx %i\n", 
            load_inst->readPC(), loadTail);

    load_inst->lqIdx = loadTail;
    if (stores == 0) {
        load_inst->sqIdx = -1;
    } else {
        load_inst->sqIdx = storeTail;
    }

    loadQueue[loadTail++] = load_inst;

    if (loadTail == LQEntries) {
        loadTail -= LQEntries;
    }

    ++loads;
}

template <class Impl>
void
LDSTQ<Impl>::insertStore(DynInstPtr &store_inst)
{
    assert((storeTail + 1) % SQEntries != storeHead && 
           (stores + storesToWB) < SQEntries);

    DPRINTF(LDSTQ, "LDSTQ: Inserting store PC %#x, idx %i\n", 
            store_inst->readPC(), storeTail);

    store_inst->sqIdx = storeTail;
    store_inst->lqIdx = loadTail;

    storeQueue[storeTail++] = SQEntry(store_inst);

    if (storeTail == SQEntries) {
        storeTail -= SQEntries;
    }

    ++stores;
}

#if 0
template <class Impl>
void
LDSTQ<Impl>::setAddr(DynInstPtr &inst)
{
    // Make sure we really have a memory reference.
    assert(inst->isMemRef());

    // Make sure it's one of the two classes of memory references.
    assert(inst->isLoad() || inst->isStore());

    if (inst->isLoad()) {
        setLoadAddr(inst);
    } else {
        setStoreAddr(inst);
    }
}

template <class Impl>
void
LDSTQ<Impl>::setLoadAddr(DynInstPtr &load_inst)
{
    
}

template <class Impl>
void
LDSTQ<Impl>::setStoreAddr(DynInstPtr &store_inst)
{
}
#endif

template <class Impl>
int
LDSTQ<Impl>::numLoadsReady()
{
    int load_idx = loadHead;
    int retval = 0;

    while (load_idx != loadTail) {
        assert(loadQueue[load_idx]);

        if (loadQueue[load_idx]->readyToIssue()) {
            ++retval;
        }
    }

    return retval;
}

#if 0
template <class Impl>
Fault
LDSTQ<Impl>::executeLoad()
{
    Fault load_fault = No_Fault;
    DynInstPtr load_inst;

    assert(readyLoads.size() != 0);

    // Execute a ready load.
    ld_map_it_t ready_it = readyLoads.begin();

    load_inst = (*ready_it).second;

    // Execute the instruction, which is held in the data portion of the
    // iterator.
    load_fault = load_inst->execute();

    // If it executed successfully, then switch it over to the executed
    // loads list.
    if (load_fault == No_Fault) {
        executedLoads[load_inst->seqNum] = load_inst;

        readyLoads.erase(ready_it);
    } else {
        loadFaultInst = load_inst;
    }

    return load_fault;
}
#endif

template <class Impl>
Fault
LDSTQ<Impl>::executeLoad(DynInstPtr &inst)
{
    // Execute a specific load.
    Fault load_fault = No_Fault;

    DPRINTF(LDSTQ, "LDSTQ: Executing load PC %#x\n", inst->readPC());

    // Make sure it's really in the list.
    // Normally it should always be in the list.  However,
    // due to a syscall it may not be the list.
#ifdef DEBUG
    int i = loadHead;
    while (1) {
        if (i == loadTail) {
            assert(0 && "Load not in the queue!");
        } else if (loadQueue[i] == inst) {
            break;
        } 

        i = i + 1;
        if (i >= LQEntries) {
            i = 0;
        }
    }
#endif // DEBUG

    load_fault = inst->execute();

    // Might want to make sure that I'm not overwriting a previously faulting
    // instruction that hasn't been checked yet.
    // Actually probably want the oldest faulting load
    if (load_fault != No_Fault) {
        if (!loadFaultInst || loadFaultInst->seqNum > inst->seqNum) {
            loadFaultInst = inst;
        }
    }

    return load_fault;
}

template <class Impl>
Fault
LDSTQ<Impl>::executeStore(DynInstPtr &store_inst)
{
    // Make sure that a store exists.
    assert(stores != 0);

    int store_idx = store_inst->sqIdx;

    DPRINTF(LDSTQ, "LDSTQ: Executing store PC %#x\n", store_inst->readPC());

    // Check the recently completed loads to see if any match this store's
    // address.  If so, then we have a memory ordering violation.
    int load_idx = store_inst->lqIdx;

    Fault store_fault = store_inst->execute();

    // Store size should now be available.  Use it to get proper offset for
    // addr comparisons.
    int size = storeQueue[store_idx].size;

    if (size == 0)
        return store_fault;

    // Optimize this so it only needs to look at ones that are younger
    // than itself. (which is actually every load anyways..)
    if (!memDepViolator) {
        while (load_idx != loadTail) 
        {
            // Must actually check all addrs in the proper size range
            if ((loadQueue[load_idx]->effAddr & ~(size - 1)) ==
                (store_inst->effAddr & ~(size - 1))) {
                // A load incorrectly passed this store.  Squash and refetch.
                // For now return a fault to show that it was unsuccessful.
                memDepViolator = loadQueue[load_idx];

                return Machine_Check_Fault;
            }

            load_idx = load_idx + 1;
            if (load_idx >= LQEntries) {
                load_idx = 0;
            }
        }

        // If we've reached this point, there was no violation.
        memDepViolator = NULL;
    }

    if (!storeFaultInst) {
        if (store_fault == No_Fault || store_fault == Fake_Mem_Fault) {
            storeFaultInst = NULL;
        } else {
            assert(0 && "Fault in a store instruction!");
            storeFaultInst = store_inst;
        }
    }

    return store_fault;
}

template <class Impl>
void
LDSTQ<Impl>::commitLoad()
{
    assert(loadQueue[loadHead]);

    DPRINTF(LDSTQ, "LDSTQ: Committing head load instruction, PC %#x\n",
            loadQueue[loadHead]->readPC());

    loadQueue[loadHead] = NULL;

    loadHead = loadHead + 1; 
    if (loadHead == LQEntries) {
        loadHead = 0;
    }

    --loads;
}

template <class Impl>
void
LDSTQ<Impl>::commitLoad(InstSeqNum &inst)
{
    // Hopefully I don't use this function too much
    panic("LDSTQ: Don't use this function!");
    int i = loadHead;
    while (1) {
        if (i == loadTail) {
            assert(0 && "Load not in the queue!");
        } else if (loadQueue[i]->seqNum == inst) {
            break;
        } 

        ++i;
        if (i >= LQEntries) {
            i = 0;
        }
    }

    loadQueue[i] = NULL;

    --loads;
}

template <class Impl>
void
LDSTQ<Impl>::commitLoads(InstSeqNum &youngest_inst)
{
    assert(loads == 0 || loadQueue[loadHead]);

    while (loads != 0 && loadQueue[loadHead]->seqNum <= youngest_inst) {
        commitLoad();
    }
}

template <class Impl>
void
LDSTQ<Impl>::commitStores(InstSeqNum &youngest_inst)
{
    assert(stores == 0 || storeQueue[storeHead].inst);

    int store_idx = storeHead;

    while (store_idx != storeTail) {
        if (!storeQueue[store_idx].canWB) {
            if (storeQueue[store_idx].inst->seqNum > youngest_inst) {
                break;
            }
            DPRINTF(LDSTQ, "LDSTQ: Marking store as able to write back, PC "
                    "%#x\n",
                    storeQueue[store_idx].inst->readPC());

            storeQueue[store_idx].canWB = true;

            ++storesToWB;
        }

        if (++store_idx == SQEntries) {
            store_idx = 0;
        }
    }
}

template <class Impl>
void
LDSTQ<Impl>::writebackStores()
{
    while (storesToWB > 0 && 
           storeQueue[storeHead].canWB && 
           usedPorts < cachePorts) {
        if (dcacheInterface && dcacheInterface->isBlocked()) {
            DPRINTF(LDSTQ, "LDSTQ: Unable to write back any more stores, cache "
                    "is blocked!\n");
            break;
        }

        ++usedPorts;

        --stores;
        --storesToWB;

        if (storeQueue[storeHead].inst->isDataPrefetch()) {
            if (++storeHead >= SQEntries) {
                storeHead = 0;
            }
            continue;
        }

        assert(storeQueue[storeHead].req);

        DPRINTF(LDSTQ, "LDSTQ: Writing back store PC %#x\n", 
                storeQueue[storeHead].inst->readPC());

        MemReqPtr req = storeQueue[storeHead].req;
        storeQueue[storeHead].committed = true;

        req->cmd = Write;
        req->completionEvent = NULL;
        req->expectCompletionEvent = false;
        req->time = curTick;

        if (dcacheInterface) {
            dcacheInterface->access(req);
        }

        // Can I just do the functional access back to back, or do I need a
        // specific writeback event to complete the rest of this?

        switch(storeQueue[storeHead].size) {
          case 1:
            cpu->write(req, (uint8_t &)storeQueue[storeHead].data);
            break;
          case 2: 
            cpu->write(req, (uint16_t &)storeQueue[storeHead].data);
            break;
          case 4:
            cpu->write(req, (uint32_t &)storeQueue[storeHead].data);
            break;
          case 8:
            cpu->write(req, (uint64_t &)storeQueue[storeHead].data);
            break;
          default:
            panic("Unexpected store size!\n");
        }

        if (++storeHead >= SQEntries) {
            storeHead = 0;
        }
    }

    usedPorts = 0;

    assert(stores >= 0 && storesToWB >= 0);
}


template <class Impl>
void
LDSTQ<Impl>::squash(const InstSeqNum &squashed_num)
{
    DPRINTF(LDSTQ, "LDSTQ: Squashing!\n");

    int load_idx = (loadTail - 1);
    if (load_idx < 0) {
        load_idx += LQEntries;
    }

    while (loads != 0 && loadQueue[load_idx]->seqNum > squashed_num) {
        // Clear the smart pointer to make sure it is decremented.
        loadQueue[load_idx] = NULL;
        --loads;

        // Inefficient!
        loadTail = load_idx;

        load_idx = load_idx - 1;
        if (load_idx < 0) {
            load_idx += LQEntries;
        }
    }

    int store_idx = (storeTail - 1);
    if (store_idx < 0) {
        store_idx += SQEntries;
    }

    while (stores != 0 && storeQueue[store_idx].inst->seqNum > squashed_num) {
        // Clear the smart pointer to make sure it is decremented.
        storeQueue[store_idx].inst = NULL;
        storeQueue[store_idx].canWB = 0;
        --stores;

        // Inefficient!
        storeTail = store_idx;

        store_idx = store_idx - 1;
        if (store_idx < 0) {
            store_idx += SQEntries;
        }
    }

}

template <class Impl>
typename Impl::DynInstPtr
LDSTQ<Impl>::getMemDepViolator()
{
    DynInstPtr temp = memDepViolator;
    
    memDepViolator = NULL;
    
    return temp;
}

template <class Impl>
bool
LDSTQ<Impl>::violation()
{
    return memDepViolator;
}

template <class Impl>
void
LDSTQ<Impl>::dumpInsts()
{
    cprintf("Load store queue: Dumping instructions.\n");
    cprintf("Load queue size: %i\n", loads);
    cprintf("Load queue: ");

    int load_idx = loadHead;

    while (load_idx != loadTail && loadQueue[load_idx]) {
        cprintf("%#x ", loadQueue[load_idx]->readPC());

        load_idx = load_idx + 1;
        if (load_idx >= LQEntries) {
            load_idx = 0;
        }
    }


    cprintf("Store queue size: %i\n", stores);
    cprintf("Store queue: ");

    int store_idx = storeHead;

    while (store_idx != storeTail && storeQueue[store_idx].inst) {
        cprintf("%#x ", storeQueue[store_idx].inst->readPC());

        store_idx = store_idx + 1;
        if (store_idx >= SQEntries) {
            store_idx = 0;
        }
    }

    cprintf("\n");
}

#if !FULL_SYSTEM
template <class Impl>
void
LDSTQ<Impl>::writebackAllInsts()
{
    while (storesToWB > 0 && 
           storeQueue[storeHead].canWB) {
        if (dcacheInterface && dcacheInterface->isBlocked()) {
            DPRINTF(LDSTQ, "LDSTQ: Unable to write back any more stores, cache "
                    "is blocked!\n");
            break;
        }

        assert(storeQueue[storeHead].req);

        DPRINTF(LDSTQ, "LDSTQ: Writing back store PC %#x\n", 
                storeQueue[storeHead].inst->readPC());

        MemReqPtr req = storeQueue[storeHead].req;
        storeQueue[storeHead].committed = true;

        req->cmd = Write;
        req->completionEvent = NULL;
        req->expectCompletionEvent = false;
        req->time = curTick;

        if (dcacheInterface) {
            dcacheInterface->access(req);
        }

        cpu->write(req, storeQueue[storeHead].data);
        // Can I just do the functional access back to back, or do I need a
        // specific writeback event to complete the rest of this?

        if (++storeHead >= SQEntries) {
            storeHead = 0;
        }
        --stores;
        --storesToWB;
    }

    usedPorts = 0;
}
#endif
