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

#include <cstdlib>

// #define DO_MEMTEST

using namespace std;

template<class Mem>
MemoryInterface<Mem>::MemoryInterface(const string &name, HierParams *hier,
				      Mem *_mem, MemTraceWriter *mem_trace)
    : MemInterface(name, hier, _mem->getHitLatency(), _mem->getBlockSize()),
      mem(_mem), memTrace(mem_trace)
{
    thisName = name;
    issuedWarning = false;

#ifdef DO_MEMTEST
    testEvent = new MemoryInterfaceTestEvent(this);
    testEvent->schedule(0);
#endif
}

template<class Mem>
MemAccessResult
MemoryInterface<Mem>::access(MemReqPtr &req)
{
#ifdef DO_MEMTEST
    if(!req->isMemTestReq) return MA_CACHE_MISS;
#endif
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

    req->enteredMemSysAt = curTick;

    if(mem->isCache() && req->cmd == Write){
    	req->isStore = true;
    }

    if(mem->isMultiprogWorkload && mem->isCache()){

        /* move each application into its separate address space */
        int cpuId = mem->memoryAddressOffset;
        int cpu_count = mem->memoryAddressParts;

        req->oldAddr = req->paddr;
		req->paddr = relocateAddrForCPU(cpuId, req->paddr, cpu_count);

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

#ifdef DO_MEMTEST
    if(req->isMemTestReq){
        cout << curTick << " " << name() << ": Response to addr " << hex << req->paddr << dec << " recieved at " << time << ", latency " << time - req->time << "\n";
        return;
    }
#endif

#ifdef CACHE_DEBUG
    if(mem->isCache()){
        if (!req->prefetched) {
            mem->removePendingRequest(req->paddr, req);
      }
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

template<class Mem>
void
MemoryInterface<Mem>::handleTestEvent(Tick time){

    ifstream fin("../../mem/testscripts/simplemisses.test");
    assert(fin);

    string line;
    vector<vector<string> > actions;
    while(getline(fin, line)){
        vector<string> tokens;
        int index = line.find_first_of(' ');
        while(index != string::npos){
            tokens.push_back(line.substr(0, index));
            line = line.replace(0, index+1, "");
            index = line.find_first_of(' ');
        }
        tokens.push_back(line);
        actions.push_back(tokens);
    }

    for(int i=0;i<actions.size();i++){
        if((actions[i][0] == "*" || actions[i][0] == mem->name())
            && (mem->isInstructionCache() ? actions[i][1] == "I" : actions[i][1] == "D")){
            MemReqPtr req = new MemReq();

            req->paddr = (Addr) atoi(actions[i][3].c_str());
            req->asid = 0;
            req->size = 64;
            req->data = new uint8_t[req->size];
            req->isMemTestReq = true;

            if(actions[i][4] == "Write") req->cmd = Write;
            else req->cmd = Read;

            Tick at = (Tick) atoi(actions[i][2].c_str());
            req->time = at;
            MemoryInterfaceTestActionEvent* evt = new MemoryInterfaceTestActionEvent(this, req);
            evt->schedule(at);
            testRequests.push_back(req);

            cout << name() << ": Creating request addr " << hex << req->paddr << dec << ", cmd " << req->cmd << " @ " << at << "\n";
        }
    }
}
