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

//#define DO_MISS_COUNT_TRACE

#define FAST_MLP_ESTIMATION

#include "mem/cache/miss/mshr_queue.hh"
#include "sim/eventq.hh"

using namespace std;

MSHRQueue::MSHRQueue(int num_mshrs, bool _isMissQueue, int reserve)
: numMSHRs(num_mshrs + reserve - 1), numReserve(reserve)
{
	isMissQueue = _isMissQueue;

	maxMSHRs = num_mshrs;

	lastMSHRChangeAt = 0;
	allocated = 0;
	inServiceMSHRs = 0;
	allocatedTargets = 0;
	registers = new MSHR[numMSHRs];
	for (int i = 0; i < numMSHRs; ++i) {
		freeList.push_back(&registers[i]);
	}

	minMSHRAddr = &registers[0];
	maxMSHRAddr = &registers[numMSHRs-1];

	countdownCounter = 0;
	ROBSize = 128; // TODO: this is a hack, fix

#ifndef FAST_MLP_ESTIMATION
	mlpEstEvent = new MLPEstimationEvent(this);
	//FIXME: write a better performance mlp estimation procedure!
	mlpEstEvent->schedule(curTick+1);
#endif

	currentMLPAccumulator.resize(maxMSHRs+1, 0.0);
	mlpAccumulatorTicks = 0;
	stallMissCountAccumulator.resize(maxMSHRs+1, 0.0);
	detectedStallCycleAccumulator = 0;

	aggregateMLPAccumulator.resize(maxMSHRs+1, 0.0);
	aggregateMLPAccumulatorTicks = 0;

	outstandingMissAccumulator = 0;
	outstandingMissAccumulatorCount = 0;

	instTraceMWSAccumulator = 0;
	instTraceMWSCount = 0;

	instTraceMLPAccumulator = 0;
	instTraceMLPCount = 0;

	responsesWhileStalled = 0;
	instTraceRespWhileStalled = 0;
}

void
MSHRQueue::setNumMSHRs(int newMSHRSize){
	numMSHRs = newMSHRSize + numReserve - 1;
}

void
MSHRQueue::setCache(BaseCache* _cache){
	cache = _cache;

	for (int i = 0; i < numMSHRs; ++i) {
		registers[i].setCache(_cache);
	}

#ifdef DO_MISS_COUNT_TRACE
	if(isMissQueue && cache->adaptiveMHA != NULL && !cache->isShared){
		missCountTrace = RequestTrace(cache->name(), "MissCountTrace", 1);
		vector<string> headers;
		headers.push_back("Outstanding Misses");
		missCountTrace.initalizeTrace(headers);
	}
#endif
}

MSHRQueue::~MSHRQueue()
{
	delete [] registers;

	assert(!mlpEstEvent->scheduled());
	delete mlpEstEvent;
}

void
MSHRQueue::regStats(const char* subname){

	using namespace Stats;

	opacu_overlapped_misses
		.name(cache->name() + subname + ".opacu_overlapped_misses")
		.desc("the number of misses that overlapped with the previous miss in the ROB")
		;

	opacu_serial_misses
		.name(cache->name() + subname + ".opacu_serial_misses")
		.desc("the number of misses that did not overlap with the previous miss")
		;

	opacu_serial_percentage
		.name(cache->name() + subname + ".opacu_serial_percentage")
		.desc("the ratio of serial misses over all misses")
		;

	opacu_serial_percentage = opacu_serial_misses / (opacu_overlapped_misses+opacu_serial_misses);


	mshrcnt_overlapped_misses
		.name(cache->name() + subname + ".mshrcnt_overlapped_misses")
		.desc("the number of misses that overlapped with the previous miss in the ROB")
		;

	mshrcnt_serial_misses
		.name(cache->name() + subname + ".mshrcnt_serial_misses")
		.desc("the number of misses that did not overlap with the previous miss")
		;

	mshrcnt_serial_percentage
		.name(cache->name() + subname + ".mshrcnt_serial_percentage")
		.desc("the ratio of serial misses over all misses")
		;

	mshrcnt_serial_percentage = mshrcnt_serial_misses / (mshrcnt_serial_misses+mshrcnt_overlapped_misses);


	roblookup_overlapped_misses
		.name(cache->name() + subname + ".roblookup_overlapped_misses")
		.desc("the number of misses that overlapped with the previous miss in the ROB")
		;

	roblookup_serial_misses
		.name(cache->name() + subname + ".roblookup_serial_misses")
		.desc("the number of misses that did not overlap with the previous miss")
		;

	roblookup_serial_percentage
		.name(cache->name() + subname + ".roblookup_serial_percentage")
		.desc("the ratio of serial misses over all misses")
		;

	roblookup_serial_percentage = roblookup_serial_misses / (roblookup_serial_misses+roblookup_overlapped_misses);

	mlp_cost_accumulator
		.name(cache->name()+ subname  + ".mlp_cost_accumulator")
		.desc("accumulated MLP cost (from Qureshi et al. ISCA2006)")
		;

	mlp_cost_total_misses
		.name(cache->name() + subname + ".mlp_cost_total_misses")
		.desc("number of misses in mlp-cost measurement(from Qureshi et al. ISCA2006)")
		;

	avg_mlp_cost_per_miss
		.name(cache->name() + subname + ".avg_mlp_cost_per_miss")
		.desc("average mlp-cost measurement per miss (in clock cycles) (from Qureshi et al. ISCA2006)")
		;

	avg_mlp_cost_per_miss = mlp_cost_accumulator / mlp_cost_total_misses;

	int maxval = 250;
	if(isMissQueue) maxval = 5000;

	mlp_cost_distribution
		.init(0, maxval, 250)
		.name(cache->name() + subname +".mlp_cost_distribution")
		.desc("MLP cost distribution")
		.flags(total | pdf | cdf)
		;

    latency_distribution
		.init(0, maxval, 250)
		.name(cache->name() + subname +".latency_distribution")
		.desc("Roundtrip latency distribution")
		.flags(total | pdf | cdf)
		;


    int mshrMaxVal = 1;
	if(isMissQueue) mshrMaxVal = 32;
    allocated_mshrs_distribution
		.init(0, mshrMaxVal, 1)
		.name(cache->name() + subname  + ".allocated_mshrs_distribution")
		.desc("Allocated mshrs distribution")
		.flags(total | pdf | cdf)
		;

    mlp_estimation_accumulator
		.init(maxMSHRs+1)
    	.name(cache->name() + subname  + ".mlp_estimation_accumulator")
    	.desc("Accumulated estimated mlp with fewer MSHRs")
    	;

    mlp_active_cycles
		.name(cache->name() + subname  + ".mlp_active_cycles")
		.desc("The total number of cycles a miss was outstanding")
		;

    avg_mlp_estimation
		.name(cache->name() + subname  + ".avg_mlp_estimation")
		.desc("Estimated mlp with fewer MSHRs")
		;

    avg_mlp_estimation = mlp_estimation_accumulator / mlp_active_cycles;
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
	allocatedMSHRsChanged(true);

	missArrived(req->cmd);

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

	missArrived(Read);

	allocated += 1;
	allocatedMSHRsChanged(true);

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
	allocatedMSHRsChanged(true);

	missArrived(Read);

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

	if(mshr->mlpCost != 0 && isMissQueue && !cache->isShared){

		int latency = curTick - (mshr->req->time + cache->getHitLatency());
		assert(mshr->mlpCost <= latency);

		mlp_cost_accumulator += mshr->mlpCost;
		mlp_cost_total_misses += 1;

		mlp_cost_distribution.sample(mshr->mlpCost);
		latency_distribution.sample(latency);

		for(int i=0;i<=maxMSHRs;i++){
			mlp_estimation_accumulator[i] += mshr->mlpCostDistribution[i];
			currentMLPAccumulator[i] += mshr->mlpCostDistribution[i];
		}
		mlpAccumulatorTicks += latency;
		mlp_active_cycles += latency;
	}

	if(isMissQueue && !cache->isShared && cache->interferenceManager != NULL){

		if(cache->interferenceManager->isStalledForMemory(cache->cacheCpuID)){
			responsesWhileStalled++;
			instTraceRespWhileStalled++;
		}
	}

	MSHR::Iterator retval = allocatedList.erase(mshr->allocIter);
	freeList.push_front(mshr);
	allocated--;
	allocatedMSHRsChanged(false);

	allocatedTargets -= mshr->getNumTargets();
	if (mshr->inService) {
		inServiceMSHRs--;
	} else {
		pendingList.erase(mshr->readyIter);
	}
	mshr->deallocate();

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
MSHRQueue::missArrived(MemCmd cmd){

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
			assert(mshr->req->cmd == Read || mshr->req->cmd == Write || mshr->req->cmd == Soft_Prefetch);
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

bool
MSHRQueue::isDemandRequest(MemReqPtr&  req){

	MemCmd cmd = req->cmd;

	assert(cmd == Read || cmd == Write || cmd == Soft_Prefetch);

//	if(req->isSWPrefetch) return false;
	return cmd == Read || cmd == Write;
}

void
MSHRQueue::allocatedMSHRsChanged(bool increased){
#ifdef FAST_MLP_ESTIMATION
	if(!cache->isShared && isMissQueue){

		int periodMSHRs = allocated+1;
		if(increased) periodMSHRs = allocated-1;

		if(periodMSHRs > 0){
			Tick allocLength = curTick - lastMSHRChangeAt;

			double mlpcost = 1 / (double) periodMSHRs;

			for(int j=1;j<=maxMSHRs;j++){
				if(j<periodMSHRs){
					double estimatedCost = 1.0 / (double) j;
					aggregateMLPAccumulator[j] += (estimatedCost * allocLength);
				}
				else{
					aggregateMLPAccumulator[j] += (mlpcost * allocLength);
				}
			}
			aggregateMLPAccumulatorTicks += allocLength;

			instTraceMLPAccumulator += (mlpcost * allocLength);
			instTraceMLPCount += allocLength;
		}

		lastMSHRChangeAt = curTick;
	}
#endif
}

void
MSHRQueue::handleMLPEstimationEvent(){

	allocated_mshrs_distribution.sample(allocated);

	if(!cache->isShared && isMissQueue){
		int demandAllocated = 0;

		if(cache->interferenceManager != NULL &&
		   cache->interferenceManager->isStalledForMemory(cache->cacheCpuID)){
			detectedStallCycleAccumulator++;
			instTraceMWSCount++;
		}

		if(allocated > 0){

			MSHR::ConstIterator i = allocatedList.begin();
			MSHR::ConstIterator end = allocatedList.end();
			for (; i != end; ++i) {
				MSHR *mshr = *i;

				// mshrs are allocated in the same cycle as the access arrives, but the miss latency
				// starts when the request is finished in the cache (i.e after hit latency cycles)
				int ticksSinceInserted = curTick - mshr->req->time;
				if(ticksSinceInserted >= cache->getHitLatency() && isDemandRequest(mshr->req)){
					demandAllocated++;
				}
			}

			if(demandAllocated > 0){
				double mlpcost = 1 / (double) demandAllocated;

				instTraceMLPAccumulator += mlpcost;
				instTraceMLPCount++;

				i = allocatedList.begin();
				end = allocatedList.end();
				for (; i != end; ++i) {
					MSHR *mshr = *i;
					int ticksSinceInserted = curTick - mshr->req->time;
					if(ticksSinceInserted >= cache->getHitLatency() && isDemandRequest(mshr->req)){
						mshr->mlpCost += mlpcost;

						if(mshr->mlpCostDistribution.empty()) mshr->mlpCostDistribution.resize(maxMSHRs+1, 0.0);
						for(int j=0;j<=maxMSHRs;j++){
							if(j<demandAllocated){
								double estimatedCost = 1.0 / (double) j;
								mshr->mlpCostDistribution[j] += estimatedCost;
							}
							else{
								mshr->mlpCostDistribution[j] += mlpcost;
							}
						}
					}
				}

				for(int j=0;j<=maxMSHRs;j++){
					if(j<demandAllocated){
						double estimatedCost = 1.0 / (double) j;
						aggregateMLPAccumulator[j] += estimatedCost;
					}
					else{
						aggregateMLPAccumulator[j] += mlpcost;
					}
				}
				aggregateMLPAccumulatorTicks++;

				if(cache->interferenceManager != NULL
				   && cache->interferenceManager->isStalledForMemory(cache->cacheCpuID)){
					for(int k=1;k<=maxMSHRs;k++){
						if(demandAllocated > k){
							stallMissCountAccumulator[k] += k;
						}
						else{
							stallMissCountAccumulator[k] += demandAllocated;

						}
					}

					instTraceMWSAccumulator += demandAllocated;
				}

			}
		}

#ifdef DO_MISS_COUNT_TRACE

		outstandingMissAccumulator += demandAllocated;
		outstandingMissAccumulatorCount++;

		int period = 100;
		if(outstandingMissAccumulatorCount == period){
			if(cache->adaptiveMHA != NULL){

				double avgOutstanding = (double) outstandingMissAccumulator / (double) outstandingMissAccumulatorCount;

				vector<RequestTraceEntry> curEntries;
				curEntries.push_back(avgOutstanding);
				missCountTrace.addTrace(curEntries);
			}
			outstandingMissAccumulator = 0;
			outstandingMissAccumulatorCount = 0;
		}
		assert(outstandingMissAccumulatorCount < period);
#endif
	}
}

std::vector<double>
MSHRQueue::getMLPEstimate(){
	vector<double> estimateSample;
	estimateSample.resize(maxMSHRs+1, 0.0);

	if(cache->useAggregateMLPEstimator){
		for(int i=0;i<estimateSample.size();i++){
			if(aggregateMLPAccumulatorTicks > 0){
				estimateSample[i] = aggregateMLPAccumulator[i] / ((double) aggregateMLPAccumulatorTicks);
			}
			aggregateMLPAccumulator[i] = 0;
		}
		aggregateMLPAccumulatorTicks = 0;
	}
	else{
		for(int i=0;i<estimateSample.size();i++){
			if(mlpAccumulatorTicks > 0){
				estimateSample[i] = currentMLPAccumulator[i] / ((double) mlpAccumulatorTicks);
			}
			currentMLPAccumulator[i] = 0;
		}
		mlpAccumulatorTicks = 0;
	}

	return estimateSample;
}

std::vector<double>
MSHRQueue::getServicedMissesWhileStalledEstimate(){
	vector<double> estimateSample(maxMSHRs+1, 0.0);

	for(int i=1;i<=maxMSHRs;i++){
		if(detectedStallCycleAccumulator > 0){
			estimateSample[i] = stallMissCountAccumulator[i] / detectedStallCycleAccumulator;
		}
		stallMissCountAccumulator[i] = 0;
	}
	detectedStallCycleAccumulator = 0;

	return estimateSample;
}

double
MSHRQueue::getInstTraceMWS(){
	double mws = 0;
	if(instTraceMWSCount > 0){
		mws = (double) instTraceMWSAccumulator / (double) instTraceMWSCount;
	}
	instTraceMWSAccumulator = 0;
	instTraceMWSCount = 0;

	return mws;
}

double
MSHRQueue::getInstTraceMLP(){

	double mlp = 0;
	if(instTraceMLPCount > 0){
		mlp = (double) instTraceMLPAccumulator / (double) instTraceMLPCount;
	}
	instTraceMLPAccumulator = 0;
	instTraceMLPCount = 0;

	return mlp;

}

int
MSHRQueue::getResponsesWhileStalled(){
	int tmp = responsesWhileStalled;
	responsesWhileStalled = 0;
	return tmp;
}

int
MSHRQueue::getInstTraceRespWhileStalled(){
	int tmp = instTraceRespWhileStalled;
	instTraceRespWhileStalled = 0;
	return tmp;
}
