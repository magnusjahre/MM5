/*
 * Copyright (c) 2003, 2004, 2005
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

/**
 * @file
 * Definition of templatized general memory interface.
 */

#include "base/stats/events.hh"
#include "mem/memory_interface.hh"
#include "mem/trace/mem_trace_writer.hh"
#include "sim/eventq.hh"
        
#include "cpu/base.hh"

#define MAX_MEM_ADDR ULL(0xffffffffffffffff)
        
using namespace std;

template<class Mem>
MemoryInterface<Mem>::MemoryInterface(const string &name, HierParams *hier, 
				      Mem *_mem, MemTraceWriter *mem_trace)
    : MemInterface(name, hier, _mem->getHitLatency(), _mem->getBlockSize()),
      mem(_mem), memTrace(mem_trace)
{
    thisName = name;
    issuedWarning = false;
}

template<class Mem>
MemAccessResult
MemoryInterface<Mem>::access(MemReqPtr &req)
{
//     if(req->paddr == 5368991752 || req->paddr == (5368991752 & ~(Addr(mem->getBlockSize()-1)))){
//         cout << curTick << " " << name() << ": block ssen, blk addr is " << (req->paddr & ~(Addr(mem->getBlockSize()-1))) << "\n";
//     }
    
    if (memTrace) {
	memTrace->writeReq(req);
    }
    if (!doEvents()){
	mem->probe(req, true);
	if (req->isSatisfied()) {
	    if (req->completionEvent != NULL) {
		req->completionEvent->schedule(curTick);
	    }
	    return MA_HIT;
	}
	fatal("Probe not satisfied with doEvents = false\n");
    } else {
	if (req->flags & UNCACHEABLE) {
	    if (req->cmd.isRead())
		recordEvent("Uncached Read");
	    if (req->cmd.isWrite())
		recordEvent("Uncached Write");
	}
    }
    
//     if(req->paddr == 4832456296ull && mem->cacheCpuID == 0){
//         cout << curTick << ": block seen at entry to memory system\n";
//     }
    
    if(mem->isMultiprogWorkload && mem->isCache()){
        
        /* move each application into its separate address space */
//         int cpuId = req->xc->cpu->params->cpu_id;
        int cpuId = mem->cacheCpuID;
        int cpu_count = mem->cpuCount;
        
        if(cpu_count > 8  && !issuedWarning){
            warn("Multiprogram workload hack has not been tested for more that 8 CPUs, proceed with caution :-)");
            issuedWarning = true;
        }
        
        Addr cpuAddrBase = (MAX_MEM_ADDR / cpu_count) * cpuId;
        
        req->oldAddr = req->paddr;
        req->paddr = cpuAddrBase + req->paddr;
        
        /* error checking */
        if(req->paddr < cpuAddrBase){
            fatal("A memory address was moved out of this CPU's memory space at the low end");
        }
        if(req->paddr >= (MAX_MEM_ADDR / cpu_count) * (cpuId+1)){
            fatal("A memory address was moved out of this CPU's memory space at the high end");
        }
    }
    
#ifdef CACHE_DEBUG
    // this is more helpfull if done after translation
    if(mem->isCache()){
        mem->addPendingRequest(req->paddr, req);
    }
#endif

    return mem->access(req);
}

template<class Mem>
void
MemoryInterface<Mem>::respond(MemReqPtr &req, Tick time)
{
#ifdef CACHE_DEBUG
    if(mem->isCache()){
        mem->removePendingRequest(req->paddr, req);
    }
#endif
    
    if(mem->isMultiprogWorkload && mem->isCache()){
        //restore the old CPU private address
        req->paddr = req->oldAddr;
    }
    
    assert(!req->cmd.isNoResponse());
    
    if (req->completionEvent != NULL) {
	// add data copying here?
        assert(req->expectCompletionEvent);
	req->completionEvent->schedule(time);
    }
    else{
        assert(!req->expectCompletionEvent);
    }
}

template<class Mem>
void
MemoryInterface<Mem>::squash(int thread_number)
    
{
    if (memTrace) {
	MemReqPtr req = new MemReq();
	// overload asid to be thread_number
	req->asid = thread_number;
	req->cmd = Squash;
	memTrace->writeReq(req);
    }
    return mem->squash(thread_number);
}
