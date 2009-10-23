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

/** @file
 * Definition of the MSHRQueue.
 */

#include "mem/cache/miss/mshr_queue.hh"
#include "sim/eventq.hh"

using namespace std;

MSHRQueue::MSHRQueue(int num_mshrs, bool _isMissQueue, int reserve)
: numMSHRs(num_mshrs + reserve - 1), numReserve(reserve)
{
	isMissQueue = _isMissQueue;

	allocated = 0;
	inServiceMSHRs = 0;
	allocatedTargets = 0;
	registers = new MSHR[numMSHRs];
	for (int i = 0; i < numMSHRs; ++i) {
		freeList.push_back(&registers[i]);
	}

	minMSHRAddr = &registers[0];
	maxMSHRAddr = &registers[numMSHRs-1];

	allocationChangedAt = 0;
	lastAllocation = 0;

	countdownCounter = 0;
	ROBSize = 128; // TODO: this is a hack, fix
}

MSHRQueue::~MSHRQueue()
{
	delete [] registers;
}

void
MSHRQueue::regStats(){
	mlp_active_cycles
		.name(cache->name() + ".mlp_active_cycles")
		.desc("the number of cycles where at least one miss was allocated")
		;

	mlp_accumulator
		.name(cache->name() + ".mlp_accumulator")
		.desc("accumulated 1/(alloced mshrs)")
		;

	avg_mlp
		.name(cache->name() + ".average_mlp")
		.desc("the percentage of the average miss latency that the CPU is exposed to")
		;

	avg_mlp = mlp_accumulator / mlp_active_cycles;

	opacu_overlapped_misses
		.name(cache->name() + ".opacu_overlapped_misses")
		.desc("the number of misses that overlapped with the previous miss in the ROB")
		;

	opacu_serial_misses
		.name(cache->name() + ".opacu_serial_misses")
		.desc("the number of misses that did not overlap with the previous miss")
		;

	opacu_serial_percentage
		.name(cache->name() + ".opacu_serial_percentage")
		.desc("the ratio of serial misses over all misses")
		;

	opacu_serial_percentage = opacu_serial_misses / (opacu_overlapped_misses+opacu_serial_misses);


	mshrcnt_overlapped_misses
		.name(cache->name() + ".mshrcnt_overlapped_misses")
		.desc("the number of misses that overlapped with the previous miss in the ROB")
		;

	mshrcnt_serial_misses
		.name(cache->name() + ".mshrcnt_serial_misses")
		.desc("the number of misses that did not overlap with the previous miss")
		;

	mshrcnt_serial_percentage
		.name(cache->name() + ".mshrcnt_serial_percentage")
		.desc("the ratio of serial misses over all misses")
		;

	mshrcnt_serial_percentage = mshrcnt_serial_misses / (mshrcnt_serial_misses+mshrcnt_overlapped_misses);


	roblookup_overlapped_misses
		.name(cache->name() + ".roblookup_overlapped_misses")
		.desc("the number of misses that overlapped with the previous miss in the ROB")
		;

	roblookup_serial_misses
		.name(cache->name() + ".roblookup_serial_misses")
		.desc("the number of misses that did not overlap with the previous miss")
		;

	roblookup_serial_percentage
		.name(cache->name() + ".roblookup_serial_percentage")
		.desc("the ratio of serial misses over all misses")
		;

	roblookup_serial_percentage = roblookup_serial_misses / (roblookup_serial_misses+roblookup_overlapped_misses);
}

MemReqPtr
MSHRQueue::getReq() const
{
	if (pendingList.empty()) {
		return NULL;
	}

	MSHR* mshr = pendingList.front();
	assert(mshr >= minMSHRAddr && mshr <= maxMSHRAddr);

	return mshr->req;
}

MSHR*
MSHRQueue::findMatch(Addr addr, int asid) const
{

	MSHR::ConstIterator i = allocatedList.begin();
	MSHR::ConstIterator end = allocatedList.end();
	for (; i != end; ++i) {
		MSHR *mshr = *i;
		assert(mshr >= minMSHRAddr && mshr <= maxMSHRAddr);
		if (mshr->addr == addr) {
			return mshr;
		}
	}
	return NULL;
}


//HACK by Magnus
MSHR*
MSHRQueue::findMatch(Addr addr, MemCmd cmd) const
{
	assert(cmd == Read || cmd == Write);
	MSHR::ConstIterator i = allocatedList.begin();
	MSHR::ConstIterator end = allocatedList.end();

	for (; i != end; ++i) {
		MSHR *mshr = *i;
		assert(mshr >= minMSHRAddr && mshr <= maxMSHRAddr);
		if (mshr->addr == addr && mshr->directoryOriginalCmd == cmd) {
			return mshr;
		}
	}
	return NULL;
}

bool
MSHRQueue::findMatches(Addr addr, int asid, vector<MSHR*>& matches) const
{
	// Need an empty vector
	assert(matches.empty());
	bool retval = false;
	MSHR::ConstIterator i = allocatedList.begin();
	MSHR::ConstIterator end = allocatedList.end();
	for (; i != end; ++i) {
		MSHR *mshr = *i;
		assert(mshr >= minMSHRAddr && mshr <= maxMSHRAddr);
		if (mshr->addr == addr) {
			retval = true;
			matches.push_back(mshr);
		}
	}
	return retval;

}

MSHR*
MSHRQueue::findPending(MemReqPtr &req) const
{

	MSHR::ConstIterator i = pendingList.begin();
	MSHR::ConstIterator end = pendingList.end();
	for (; i != end; ++i) {
		MSHR *mshr = *i;
		assert(mshr >= minMSHRAddr && mshr <= maxMSHRAddr);
		if (mshr->addr < req->paddr) {
			if (mshr->addr + mshr->req->size > req->paddr) {
				return mshr;
			}
		} else {
			if (req->paddr + req->size > mshr->addr) {
				return mshr;
			}
		}

		//need to check destination address for copies.
		if (mshr->req->cmd == Copy) {
			Addr dest = mshr->req->dest;
			if (dest < req->paddr) {
				if (dest + mshr->req->size > req->paddr) {
					return mshr;
				}
			} else {
				if (req->paddr + req->size > dest) {
					return mshr;
				}
			}
		}
	}
	return NULL;
}

MSHR*
MSHRQueue::allocate(MemReqPtr &req, int size)
{

	Addr aligned_addr = req->paddr & ~((Addr)size - 1);
	MSHR *mshr = freeList.front();
	assert(mshr >= minMSHRAddr && mshr <= maxMSHRAddr);
	assert(mshr->getNumTargets() == 0);
	freeList.pop_front();

	if (req->cmd.isNoResponse()) {
		mshr->allocateAsBuffer(req);
	} else {
		assert(size !=0);
		mshr->allocate(req->cmd, aligned_addr, req->asid, size, req);
		allocatedTargets += 1;
	}
	mshr->allocIter = allocatedList.insert(allocatedList.end(), mshr);
	mshr->readyIter = pendingList.insert(pendingList.end(), mshr);

	allocated += 1;

	updateMLPStatistics();
	missArrived();

	return mshr;
}

MSHR*
MSHRQueue::allocateFetch(Addr addr, int asid, int size, MemReqPtr &target)
{

	MSHR *mshr = freeList.front();
	assert(mshr >= minMSHRAddr && mshr <= maxMSHRAddr);
	assert(mshr->getNumTargets() == 0);
	freeList.pop_front();
	mshr->allocate(Read, addr, asid, size, target);
	mshr->allocIter = allocatedList.insert(allocatedList.end(), mshr);
	mshr->readyIter = pendingList.insert(pendingList.end(), mshr);

	updateMLPStatistics();
	missArrived();

	allocated += 1;
	return mshr;
}

MSHR*
MSHRQueue::allocateTargetList(Addr addr, int asid, int size)
{
	MSHR *mshr = freeList.front();
	assert(mshr >= minMSHRAddr && mshr <= maxMSHRAddr);
	assert(mshr->getNumTargets() == 0);
	freeList.pop_front();
	MemReqPtr dummy;
	mshr->allocate(Read, addr, asid, size, dummy);
	mshr->allocIter = allocatedList.insert(allocatedList.end(), mshr);
	mshr->inService = true;
	++inServiceMSHRs;
	++allocated;

	missArrived();
	updateMLPStatistics();

	return mshr;
}


void
MSHRQueue::deallocate(MSHR* mshr)
{
	deallocateOne(mshr);
}

MSHR::Iterator
MSHRQueue::deallocateOne(MSHR* mshr)
{

	assert(mshr >= minMSHRAddr && mshr <= maxMSHRAddr);
	//HACK by Magnus
	mshr->directoryOriginalCmd = InvalidCmd;

	MSHR::Iterator retval = allocatedList.erase(mshr->allocIter);
	freeList.push_front(mshr);
	allocated--;
	allocatedTargets -= mshr->getNumTargets();
	if (mshr->inService) {
		inServiceMSHRs--;
	} else {
		pendingList.erase(mshr->readyIter);
	}
	mshr->deallocate();

	updateMLPStatistics();

	return retval;
}

void
MSHRQueue::moveToFront(MSHR *mshr)
{

	assert(mshr >= minMSHRAddr && mshr <= maxMSHRAddr);
	if (!mshr->inService) {
		assert(mshr == *(mshr->readyIter));
		pendingList.erase(mshr->readyIter);
		mshr->readyIter = pendingList.insert(pendingList.begin(), mshr);
	}
}

void
MSHRQueue::markInService(MSHR* mshr)
{

	assert(mshr >= minMSHRAddr && mshr <= maxMSHRAddr);

	if (mshr->req->cmd.isNoResponse()) {
		assert(mshr->getNumTargets() == 0);
		deallocate(mshr);
		return;
	}
	mshr->inService = true;
	pendingList.erase(mshr->readyIter);
	mshr->readyIter = (list<MSHR*>::iterator) NULL;
	inServiceMSHRs += 1;
	//pendingList.pop_front();

}

void
MSHRQueue::markPending(MSHR* mshr, MemCmd cmd)
{

	assert(mshr >= minMSHRAddr && mshr <= maxMSHRAddr);

	assert(mshr->readyIter == (list<MSHR*>::iterator) NULL);
	mshr->req->cmd = cmd;
	mshr->req->flags &= ~SATISFIED;
	mshr->inService = false;
	--inServiceMSHRs;
	/**
	 * @ todo might want to add rerequests to front of pending list for
	 * performance.
	 */
	mshr->readyIter = pendingList.insert(pendingList.end(), mshr);
}

void
MSHRQueue::squash(int thread_number)
{

	MSHR::Iterator i = allocatedList.begin();
	MSHR::Iterator end = allocatedList.end();
	for (; i != end;) {
		MSHR *mshr = *i;
		assert(mshr >= minMSHRAddr && mshr <= maxMSHRAddr);
		if (mshr->threadNum == thread_number) {
			while (mshr->hasTargets()) {
				MemReqPtr target = mshr->getTarget();
#ifdef CACHE_DEBUG
				assert(cache != NULL);
				cache->removePendingRequest(target->paddr, target);
#endif
				mshr->popTarget();

				assert(target->thread_num == thread_number);
				if (target->completionEvent != NULL) {
					delete target->completionEvent;
				}
				target = NULL;
			}
			assert(!mshr->hasTargets());
			assert(mshr->ntargets==0);
			if (!mshr->inService) {
				i = deallocateOne(mshr);
			} else {
				//mshr->req->flags &= ~CACHE_LINE_FILL;
				++i;
			}
		} else {
			++i;
		}
	}
}

std::map<int,int>
MSHRQueue::assignBlockingBlame(int maxTargets, bool blockedMSHRs, double threshold){

	MSHR::ConstIterator i = allocatedList.begin();
	MSHR::ConstIterator end = allocatedList.end();

	map<int,int> retmap;

	if(blockedMSHRs){

		vector<int> ownedBy(cache->cpuCount, 0);

		for (; i != end; ++i) {
			MSHR *mshr = *i;
			ownedBy[mshr->req->adaptiveMHASenderID]++;
		}

		for(int i=0;i<ownedBy.size();i++){
			if(ownedBy[i] >= (threshold * getCurrentMSHRCount())){
				retmap[i] = ownedBy[i];
			}
		}

		return retmap;

	}
	else{

		// with the current impl, L2 never should not block for targets

		MSHR* tgtMSHR = NULL;

		for (; i != end; ++i) {
			MSHR *mshr = *i;
			if(mshr->getNumTargets() == maxTargets){
				assert(tgtMSHR == NULL);
				tgtMSHR = mshr;
			}
		}
		assert(tgtMSHR != NULL);

		retmap[tgtMSHR->req->adaptiveMHASenderID] = 1;
		return retmap;
	}
	return retmap;

}

void
MSHRQueue::updateMLPStatistics(){

	if(lastAllocation > 0){
		Tick activeCycles = curTick - allocationChangedAt;
		float weightedValue = (1 / ((float) lastAllocation)) * activeCycles;

		mlp_accumulator += weightedValue;
		mlp_active_cycles += activeCycles;
	}

	allocationChangedAt = curTick;
	lastAllocation = allocated;
}

void
MSHRQueue::missArrived(){

	if(countdownCounter > 0){
		opacu_overlapped_misses++;
	}
	else{
		opacu_serial_misses++;
		countdownCounter = ROBSize;
	}

	// check ROB to see if the requests actually overlap
	if(cache->adaptiveMHA != NULL && !cache->isShared && isMissQueue){
		int outstandingCnt = 0;

		MSHR::ConstIterator i = allocatedList.begin();
		MSHR::ConstIterator end = allocatedList.end();
		for (; i != end; ++i) {
			MSHR *mshr = *i;
			assert(mshr->req->cmd == Read);
			bool tmpInROB = cache->adaptiveMHA->requestInROB(mshr->req, cache->cacheCpuID, cache->getBlockSize());
			if(tmpInROB) outstandingCnt++;
		}

		if(outstandingCnt > 1){
			roblookup_overlapped_misses++;
		}
		else{
			roblookup_serial_misses++;
		}
	}

	if(allocated > 1){
		mshrcnt_overlapped_misses++;
	}
	else{
		mshrcnt_serial_misses++;
	}
}

void
MSHRQueue::cpuCommittedInstruction(){

	if(countdownCounter > 0){
		countdownCounter--;
	}
	assert(countdownCounter >= 0);
}

