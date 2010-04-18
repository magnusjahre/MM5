/*
 * Copyright (c) 2002, 2003, 2004, 2005
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
 * Definition of the Bus class, DeliverEvent, and arbitration events.
 */

#include <cassert>
#include <sstream>
#include <string>
#include <vector>
#include <iostream>
#include <fstream>

#include "base/cprintf.hh"
#include "base/intmath.hh"
#include "base/misc.hh"
#include "base/callback.hh"
#include "cpu/smt.hh"
#include "mem/bus/bus.hh"
// #include "mem/bus/bus_interface.hh"
#include "sim/builder.hh"
#include "sim/host.hh"
#include "sim/stats.hh"

#include "mem/bus/controller/rdfcfs_memory_controller.hh"

#include <fstream>
#include <iomanip>

using namespace std;

/** The maximum value of type Tick. */
#define TICK_T_MAX ULL(0x3FFFFFFFFFFFFF)

// #define TRACE_QUEUE

#define MAX_ARB_RESCHED 1000

#define WARMUP_LATENCY 40

// bus constructor
Bus::Bus(const string &_name,
         HierParams *hier_params,
         int _width,
         int _clock,
         AdaptiveMHA* _adaptiveMHA,
         int _cpu_count,
         int _bank_count,
         Tick _switch_at,
         TimingMemoryController* _fwController,
         TimingMemoryController* _memoryController,
         bool _infiniteBW,
         InterferenceManager* intman,
         double _utilizationLimit)
    : BaseHier(_name, hier_params)
{
    width = _width;
    clockRate = _clock;
    cpu_count = _cpu_count;
    bank_count = _bank_count;
    infiniteBW = _infiniteBW;

    interferenceManager = intman;
    interferenceManager->registerBus(this);

    serviceCyclesSample = 0;
    requestSample = 0;
    lastSampleTick = 0;

    requestPerCoreSample.resize(cpu_count, 0);
    readsPerCoreSample.resize(cpu_count, 0);

    busInterference = vector<vector<int> >(cpu_count, vector<int>(cpu_count,0));
    conflictInterference = vector<vector<int> >(cpu_count, vector<int>(cpu_count,0));
    hitToMissInterference = vector<vector<int> >(cpu_count, vector<int>(cpu_count,0));

    if(_adaptiveMHA != NULL) adaptiveMHA = _adaptiveMHA;
    else adaptiveMHA = NULL;

    fwMemoryController = _fwController;
    simMemoryController = _memoryController;

    if (width < 1 || (width & (width - 1)) != 0)
	fatal("memory bus width must be positive non-zero and a power of two");

    if (clockRate < 1)
	fatal("memory bus clock must be positive and non-zero");

    if(_memoryController == NULL) fatal("A memory controller must be given");

    nextDataFree = 0;
    nextAddrFree = 0;

    runAddrLast = 0;
    runDataLast = 0;

    busBlockedTime = 0;

    nextfree = 0;

    livearbs = 0;

    numInterfaces = 0;
    blocked = false;
    waitingFor = -1;

    lastarbevent = 0;

    arbiter_scheduled_flag = false;

    need_to_sort = true;

    perCPUDataBusUse.resize(cpu_count, 0);
    perCPUQueueCycles.resize(cpu_count, 0);
    perCPURequests.resize(cpu_count, 0);

    outstandingReads.resize(cpu_count, 0);
    outstandingWrites.resize(cpu_count, 0);

    int rl = _memoryController->getReadQueueLength()+1;
    int wl = _memoryController->getWriteQueueLength()+1;
    queueDelaySum.resize(cpu_count, vector<vector<Tick> >(rl, vector<Tick>(wl, 0)));
    queueDelayRequests.resize(cpu_count, vector<vector<int> >(rl, vector<int>(wl, 0)));

    if(_adaptiveMHA != NULL) _adaptiveMHA->registerBus(this);

    if(_fwController != NULL) fatal("Fast forward memory controller not implemented");
    if(_memoryController == NULL) fatal("A memory controller must be provided to the memory bus");
    _memoryController->registerBus(this, _cpu_count);

    memoryController = simMemoryController;
    memoryControllerEvent = new MemoryControllerEvent(this, false, -1);

    detailedSimulationStart = _switch_at;

    MemoryBusQueuedReqEvent* qrm = new MemoryBusQueuedReqEvent(this);
    qrm->schedule(curTick+1);

    if(_utilizationLimit > 0 && cpu_count != 1) fatal("Utilization limit only makes sense for single core experiments");
    if(_utilizationLimit > 1.0) fatal("The utilization limit cannot be larger than 1.0");
    utilizationLimit = _utilizationLimit;

    trafficGenerator = NULL;

#ifdef DO_BUS_TRACE
    ofstream file("busAccessTrace.txt");
    file << "";
    file.flush();
    file.close();
#endif

#ifdef INJECT_TEST_REQUESTS
    generateRequests();
#endif

#ifdef TRACE_QUEUE

    for(int i=0;i<_cpu_count;i++){
        stringstream name;
        name << "MemoryBusQueueTrace" << i << ".txt";
        ofstream qfile(name.str().c_str());

        qfile << setw(25) << "#Reads"
              << setw(25) << "Writes"
              << setw(25) << "Queue Latency"
              << setw(25) << "Service Latency"
              << "\n";
        qfile.flush();
        qfile.close();
    }
#endif
}

Bus::~Bus()
{
    delete memoryControllerEvent;
}

/* register bus stats */
void
Bus::regStats()
{
    using namespace Stats;

    busUseCycles
        .name(name() + ".bus_use_cycles")
        .desc("Number of cycles the data bus is in use")
        ;

    busUtilization
        .name(name() + ".bus_utilization")
        .desc("Data bus utilization")
        ;

    busUtilization = busUseCycles / simTicks;

    unknownSenderCycles
        .name(name() + ".unknown_sender_bus_use_cycles")
        .desc("Number of cycles the bus was used by an unknown sender")
        ;

    unknownSenderUtilization
        .name(name() + ".unknown_sender_utilization")
        .desc("Bus utilization by unknown sender requests")
        ;

    unknownSenderUtilization = unknownSenderCycles / simTicks;

    unknownSenderRequests
        .name(name() + ".unknown_sender_requests")
        .desc("Number of requests with unknown sender")
        ;

    unknownSenderFraction
        .name(name() + ".unknown_sender_fraction")
        .desc("Percentage of requests that have an unknown origin")
        ;

    unknownSenderFraction = unknownSenderRequests / totalRequests;

    totalQueueCycles
            .name(name() + ".total_queue_cycles")
            .desc("Total number of cycles spent in memory controller queue")
            ;

    totalServiceCycles
			.name(name() + ".total_service_cycles")
			.desc("Total number of cycles spent servicing memory requests")
			;

    totalRequests
            .name(name() + ".total_requests")
            .desc("Total number of requests to memory controller")
            ;

    avgQueueCycles
            .name(name() + ".avg_queue_cycles")
            .desc("Average number of cycles each request spent in queue (not counting blocked cycles)")
            ;

    avgQueueCycles = totalQueueCycles / totalRequests;

    avgServiceCycles
			.name(name() + ".avg_service_cycles")
			.desc("Average number of cycles each request spent beeing serviced (not counting blocked cycles)")
			;

    avgServiceCycles = totalServiceCycles / totalRequests;

    accessesPerCPU.init(cpu_count);
    accessesPerCPU
            .name(name() + ".accesses_per_cpu")
            .desc("number of accesses for each CPU")
            .flags(total)
            ;

    readsPerCPU
            .init(cpu_count)
            .name(name() + ".reads_per_cpu")
            .desc("number of reads for each CPU")
            .flags(total)
            ;

    writesPerCPU
            .init(cpu_count)
            .name(name() + ".writes_per_cpu")
            .desc("number of writes for each CPU")
            .flags(total)
            ;

    pageHitsPerCPU.init(cpu_count);
    pageHitsPerCPU
            .name(name() + ".page_hits_per_cpu")
            .desc("number of page hits for each CPU")
            .flags(total)
            ;

    noCPUrequests
            .name(name() + ".no_cpu_sends")
            .desc("Counted to verify that per CPU stats got all requests")
            ;

    blockedCycles
            .name(name() + ".blocked_cycles")
            .desc("Number of cycles the memory controller was blocked")
            ;

    nullGrants
            .name(name() + ".null_grants")
            .desc("Number of times the bus was granted in error")
            ;

    queueSizeDistribution
		.init(0,128,2)
		.name(name() + ".queue_size_distribution")
		.desc("number of ticks the given number of requests was queued")
		;
}

void
Bus::addQueueLengthSample(){
	int numQueuedReqs = memoryController->getWaitingReadCount() + memoryController->getWaitingReadCount();
	queueSizeDistribution.sample(numQueuedReqs);
}

void
Bus::incConstructiveInterference(){
    fatal("deprecated");
}

void Bus::incInterferenceMisses(){
	fatal("deprecated");
}

void
Bus::resetStats()
{
}

void
Bus::requestDataBus(int id, Tick time)
{
    requestAddrBus(id, time);
}

void
Bus::requestAddrBus(int id, Tick time)
{

    assert(doEvents());
    assert(time>=curTick);
    DPRINTF(Bus, "id:%d Requesting Address Bus for cycle: %d\n", id, time);

    AddrArbiterEvent *eventptr = new AddrArbiterEvent(this,id,time);

    need_to_sort = true;

    if (!arbiter_scheduled_flag) {
        assert(arb_events.empty());
        assert(time >= curTick);
        eventptr->schedule(time);
        arbiter_scheduled_flag = true;
        currently_scheduled = eventptr;
    }
    else if (currently_scheduled->original_time > time) {
        currently_scheduled->deschedule();
        arb_events.push_back(currently_scheduled);
        assert(time >= curTick);
        eventptr->schedule(time);
        currently_scheduled = eventptr;
        assert(arbiter_scheduled_flag);
    }
    else {
        arb_events.push_back(eventptr);
    }

    livearbs++;
}

void
Bus::arbitrateAddrBus(int interfaceid, Tick requestedAt)
{
    DPRINTF(Bus, "addr bus granted to id %d\n", interfaceid);
    livearbs--;
    interfaces[interfaceid]->grantAddr(requestedAt);
}

void
Bus::arbitrateDataBus()
{
    fatal("arbitrateDataBus() called!");
}

void
Bus::viritualPrivateWriteAccess(MemReqPtr& req){

	if(curTick < detailedSimulationStart) return;

	memoryController->insertPrivateVirtualRequest(req);
}

bool
Bus::sendAddr(MemReqPtr &req, Tick origReqTime)
{
    assert(doEvents());
    if (!req) {
        // if the bus was granted in error
        nullGrants++;
        DPRINTF(Bus, "null request");
        return false;
    }

    req->memBusBlockedWaitCycles = curTick - origReqTime;

    assert(req->cmd == Read || req->cmd == Writeback);
    if(req->adaptiveMHASenderID != -1){
    	if(req->cmd == Read) readsPerCPU[req->adaptiveMHASenderID]++;
    	else writesPerCPU[req->adaptiveMHASenderID]++;
    }

    DPRINTF(Bus, "issuing req %s addr %d from id %d, name %s\n",
	    req->cmd.toString(), req->paddr,
	    req->busId, interfaces[req->busId]->name());

#ifdef INJECT_TEST_REQUESTS
    while(!testRequests.empty()){
        memoryController->insertRequest(testRequests.front());
        testRequests.pop_front();
    }
#endif

    // Warm up code that removes the effects of contention (possible to compare shared and alone configurations)
    // Infinite bandwidth, all requests are page hits
    if(curTick < detailedSimulationStart || infiniteBW){
        if (req->cmd == Read) {
            assert(req->busId < interfaces.size() && req->busId > -1);
            DeliverEvent *deliverevent = new DeliverEvent(interfaces[req->busId], req);
            deliverevent->schedule(origReqTime + WARMUP_LATENCY);
        }

        return true;
    }

    assert(req->cmd == Read || req->cmd == Writeback);
    if(req->adaptiveMHASenderID != -1){
    	if(req->cmd == Read) outstandingReads[req->adaptiveMHASenderID]++;
    	else outstandingWrites[req->adaptiveMHASenderID]++;
    }

    // assumption: no memory bus blocking in private memory system
    if(req->interferenceMissAt == 0 && req->adaptiveMHASenderID != -1){
    	req->latencyBreakdown[MEM_BUS_ENTRY_LAT] += curTick - origReqTime;
    	req->interferenceBreakdown[MEM_BUS_ENTRY_LAT] += curTick - origReqTime;

    	if(req->cmd == Read && interferenceManager != NULL){
    		interferenceManager->addLatency(InterferenceManager::MemoryBusEntry, req, curTick - origReqTime);
			interferenceManager->addInterference(InterferenceManager::MemoryBusEntry, req, curTick - origReqTime);
		}
    }

    // Insert request into memory controller
    memoryController->insertRequest(req);
    totalRequests++;

#ifdef DO_BUS_TRACE
#ifndef INJECT_TEST_REQUESTS
    assert(slaveInterfaces.size() == 1);
    writeTraceFileLine(req->paddr,
                       slaveInterfaces[0]->getMemoryBankID(req->paddr),
                       (req->paddr >> slaveInterfaces[0]->getPageSize()),
                       -1,
                       req->cmd,
                       "Request");
#endif
#endif

    // Schedule memory controller if not scheduled yet.
    if (!memoryControllerEvent->scheduled()) {
        memoryControllerEvent->schedule(curTick);
    }

    return true;
}

void
Bus::handleMemoryController(bool isShadow, int ctrlID)
{

    if (memoryController->hasMoreRequests()) {

        MemReqPtr request = memoryController->getRequest();

        DPRINTF(Bus, "sending req %s addr %d \n", request->cmd.toString(), request->paddr);

#ifdef INJECT_TEST_REQUESTS
        if(!request->isDDRTestReq && (request->cmd == Read || request->cmd == Writeback)){
            fatal("Testing is finished, stopping execution");
        }
#endif

#ifdef DO_BUS_TRACE
        assert(slaveInterfaces.size() == 1);
        writeTraceFileLine(request->paddr,
                        slaveInterfaces[0]->getMemoryBankID(request->paddr),
                        (request->paddr >> slaveInterfaces[0]->getPageSize()),
                        -1,
                        request->cmd,
                        "Send");
#endif

        if(request->cmd != Activate && request->cmd != Close){

            assert(request->inserted_into_memory_controller >= 0);
            int queue_lat = (curTick - request->inserted_into_memory_controller) + request->memBusBlockedWaitCycles;
            totalQueueCycles += queue_lat;
            if(request->adaptiveMHASenderID >= 0 && request->adaptiveMHASenderID < cpu_count){
                perCPUQueueCycles[request->adaptiveMHASenderID] += queue_lat;
                perCPURequests[request->adaptiveMHASenderID] += 1;
                int sum = 0;
                for(int i=0;i<perCPURequests.size();i++) {
                    sum += perCPURequests[i];
                }
            }
        }

        assert(slaveInterfaces.size() == 1);
        slaveInterfaces[0]->access(request);
    }
}

int
Bus::getOtherProcUseTime(MemReqPtr& req){

	if(utilizationLimit == 0.0 || req->cmd == Activate || req->cmd == Close){
		return 0;
	}

	assert(cpu_count == 1);

	int delayPeriod = slaveInterfaces[0]->getDataTransTime() / utilizationLimit;
	int otherProcUseTime = delayPeriod - slaveInterfaces[0]->getDataTransTime();

	return otherProcUseTime;
}

/* This function is called when the DRAM has calculated the latency */
void Bus::latencyCalculated(MemReqPtr &req, Tick time, bool fromShadow)
{

	int simulatedContention = getOtherProcUseTime(req);

    assert(!memoryControllerEvent->scheduled());
    assert(time >= curTick);
    memoryControllerEvent->schedule(time + simulatedContention);
    nextfree = time + simulatedContention;

    DPRINTF(Bus,
            "latency calculated req %s, addr %d, latency %d\n",
            req->cmd.toString(),
            req->paddr,
            time - curTick);

    if(req->adaptiveMHASenderID == cpu_count){
    	DPRINTF(Bus, "Generated request for addr %d finished, informing generator\n", req->paddr);
    	assert(trafficGenerator != NULL);
    	trafficGenerator->requestCompleted(time);
    	return;
    }


#ifdef DO_BUS_TRACE
    assert(slaveInterfaces.size() == 1);
    writeTraceFileLine(req->paddr,
                       slaveInterfaces[0]->getMemoryBankID(req->paddr),
                       (req->paddr >> slaveInterfaces[0]->getPageSize()),
                       time - curTick,
                       req->cmd,
                       "Latency");
#endif

    if(req->cmd != Activate && req->cmd != Close){
        assert(slaveInterfaces.size() == 1);
        busUseCycles += slaveInterfaces[0]->getDataTransTime();

        if(req->adaptiveMHASenderID != -1){
            perCPUDataBusUse[req->adaptiveMHASenderID] += slaveInterfaces[0]->getDataTransTime();
        }
        else{
            unknownSenderCycles += slaveInterfaces[0]->getDataTransTime();
        }


        totalServiceCycles += time - curTick;

        serviceCyclesSample += time - curTick;
        requestSample++;

        assert(req->cmd == Read || req->cmd == Writeback);
        if(req->adaptiveMHASenderID != -1){
        	requestPerCoreSample[req->adaptiveMHASenderID]++;
        	if(req->cmd == Read){
        		readsPerCoreSample[req->adaptiveMHASenderID]++;
        	}
        }
    }

#ifdef INJECT_TEST_REQUESTS
    // the DDR Test Requests are generated in the bus, so we don't want to return them
    if(req->isDDRTestReq) return;
#endif

    if((req->cmd == Read || req->cmd == Writeback) && req->adaptiveMHASenderID != -1){
        if(req->cmd == Read) outstandingReads[req->adaptiveMHASenderID]--;
        else outstandingWrites[req->adaptiveMHASenderID]--;
    }

    if((req->cmd == Read || req->cmd == Writeback) && cpu_count > 1 && req->interferenceMissAt == 0 && req->adaptiveMHASenderID != -1){
        memoryController->computeInterference(req, time - curTick);
    }

    if (req->cmd == Read) {

    	assert(req->adaptiveMHASenderID != -1);

        Tick serviceLatency = time - curTick;
        Tick queueLatency = curTick - req->inserted_into_memory_controller;
//         Tick totalLat = time - req->inserted_into_memory_controller;

        if(req->interferenceMissAt == 0){
			req->latencyBreakdown[MEM_BUS_QUEUE_LAT] += queueLatency;
			req->latencyBreakdown[MEM_BUS_SERVICE_LAT] += serviceLatency;

			if(interferenceManager != NULL){
				interferenceManager->addLatency(InterferenceManager::MemoryBusQueue, req, queueLatency);
				interferenceManager->addLatency(InterferenceManager::MemoryBusService, req, serviceLatency);
			}
        }

        assert(req->entryReadCnt <= memoryController->getReadQueueLength());
        assert(req->entryWriteCnt <= memoryController->getWriteQueueLength());
        queueDelaySum[req->adaptiveMHASenderID][req->entryReadCnt][req->entryWriteCnt] += queueLatency;
        queueDelayRequests[req->adaptiveMHASenderID][req->entryReadCnt][req->entryWriteCnt]++;

#ifdef TRACE_QUEUE

        stringstream name;
        name << "MemoryBusQueueTrace" << req->adaptiveMHASenderID << ".txt";
        ofstream qfile(name.str().c_str(), ofstream::app);

        qfile << setw(25) << req->entryReadCnt
              << setw(25) << req->entryWriteCnt
              << setw(25) << queueLatency
              << setw(25) << serviceLatency
              << "\n";
        qfile.flush();
        qfile.close();
#endif

        if(cpu_count > 1 && req->interferenceMissAt == 0){

            int queueInterference = queueLatency - (req->busAloneReadQueueEstimate + req->busAloneWriteQueueEstimate);
            int serviceInterference = serviceLatency - req->busAloneServiceEstimate;

            req->interferenceBreakdown[MEM_BUS_QUEUE_LAT] = queueInterference;
            req->interferenceBreakdown[MEM_BUS_SERVICE_LAT] = serviceInterference;

            if(req->cmd == Read && interferenceManager != NULL){
            	interferenceManager->addInterference(InterferenceManager::MemoryBusQueue, req, queueInterference);
            	interferenceManager->addInterference(InterferenceManager::MemoryBusService, req, serviceInterference);
            }
        }

        assert(req->busId < interfaces.size() && req->busId > -1);
        DeliverEvent *deliverevent = new DeliverEvent(interfaces[req->busId], req);
        deliverevent->schedule(time);
    }
    else if(req->cmd == Writeback && adaptiveMHA != NULL && req->adaptiveMHASenderID != -1){
        adaptiveMHA->addTotalDelay(req->adaptiveMHASenderID, time - req->writebackGeneratedAt, req->paddr, false);
    }
}

int
Bus::registerInterface(BusInterface<Bus> *bi, bool master)
{

    assert(numInterfaces == interfaces.size());
    interfaces.push_back(bi);
    addrBusRequests.push_back(BusRequestRecord());
    dataBusRequests.push_back(BusRequestRecord());

    DPRINTF(Bus, "registering interface %s as %s\n",
            bi->name(), master ? "master" : "slave");

    if (master) {
        transmitInterfaces.insert(transmitInterfaces.begin(),bi);
    } else {
        slaveInterfaces.push_back(bi);
//         fwMemoryController->registerInterface(bi);
        simMemoryController->registerInterface(bi);
        transmitInterfaces.push_back(bi);
    }

    return numInterfaces++;
}
void
Bus::collectRanges(list<Range<Addr> > &range_list)
{
    for (int i = 0; i < numInterfaces; ++i) {
	interfaces[i]->getRange(range_list);
    }
}

void
Bus::rangeChange()
{
    for (int i = 0; i < numInterfaces; ++i) {
	interfaces[i]->rangeChange();
    }
}

void
Bus::incrementBlockedCycles(Tick cycles){
    blockedCycles += cycles;
}

void
Bus::resetAdaptiveStats(){
    for(int i=0;i<perCPUDataBusUse.size();i++) perCPUDataBusUse[i] = 0;
    for(int i=0;i<perCPUQueueCycles.size();i++) perCPUQueueCycles[i] = 0;
    for(int i=0;i<perCPURequests.size();i++) perCPURequests[i] = 0;
}

double
Bus::getAverageQueue(Tick sampleSize){

    int sum = 0;
    int reqs = 0;
    for(int i=0;i<perCPUQueueCycles.size();i++) sum += perCPUQueueCycles[i];
    for(int i=0;i<perCPURequests.size();i++) reqs += perCPURequests[i];
    if(reqs == 0) return 0;
    return (double) ((double) sum / (double) reqs);
}

double
Bus::getDataBusUtilisation(Tick sampleSize){

    int sum = 0;
    for(int i=0;i<cpu_count;i++) sum += perCPUDataBusUse[i];
    return (double) ((double) sum / (double) sampleSize);
}

vector<int>
Bus::getDataUsePerCPUId(){

    vector<int> retval= perCPUDataBusUse;
    return retval;
}

vector<double>
Bus::getAverageQueuePerCPU(){

    vector<double> retval;
    retval.resize(perCPUQueueCycles.size(), 0);
    assert(perCPUQueueCycles.size() == perCPURequests.size());
    for(int i=0;i<perCPUQueueCycles.size();i++){
        if(perCPURequests[i] == 0) retval[i] = 0;
        else retval[i] = (double) ((double) perCPUQueueCycles[i] / (double) perCPURequests[i]);
    }
    return retval;
}

void
Bus::switchMemoryController(){

    DPRINTF(Bus, "Switching from fast forward controller to simulation controller\n");

    memoryController = simMemoryController;

    simMemoryController->setOpenPages(fwMemoryController->getOpenPages());

    list<MemReqPtr> pendingReqs = fwMemoryController->getPendingRequests();
    while(!pendingReqs.empty()){
        MemReqPtr tmp = pendingReqs.front();
        pendingReqs.pop_front();
        assert(tmp->cmd == Read || tmp->cmd == Writeback);
        simMemoryController->insertRequest(tmp);
    }
}

void
Bus::updatePerCPUAccessStats(int cpuID, bool pageHit){

    if(cpuID == -1){
        noCPUrequests++;
        return;
    }

    assert(cpuID >= 0 && cpuID < cpu_count);
    if(pageHit) pageHitsPerCPU[cpuID]++;
    accessesPerCPU[cpuID]++;
}

std::vector<std::vector<int> >
Bus::retrieveBusInterferenceStats(){
    return busInterference;
}

void
Bus::resetBusInterferenceStats(){
    for(int i=0;i<busInterference.size();i++){
        for(int j=0;j<busInterference.size();j++){
            busInterference[i][j] = 0;
        }
    }
}

std::vector<std::vector<int> >
Bus::retrieveConflictInterferenceStats(){
    return conflictInterference;
}

void
Bus::resetConflictInterferenceStats(){
    for(int i=0;i<conflictInterference.size();i++){
        for(int j=0;j<conflictInterference.size();j++){
            conflictInterference[i][j] = 0;
        }
    }
}

std::vector<std::vector<int> >
Bus::retrieveHitToMissInterferenceStats(){
    return hitToMissInterference;
}

void
Bus::resetHitToMissInterferenceStats(){
    for(int i=0;i<hitToMissInterference.size();i++){
        for(int j=0;j<hitToMissInterference.size();j++){
            hitToMissInterference[i][j] = 0;
        }
    }
}

void
Bus::addInterference(int victimID, int interfererID, interference_type iType){
    switch(iType){
        case BUS_INTERFERENCE:
            busInterference[victimID][interfererID]++;
            break;
        case CONFLICT_INTERFERENCE:
            conflictInterference[victimID][interfererID]++;
            break;
        case HIT_TO_MISS_INTERFERENCE:
            hitToMissInterference[victimID][interfererID]++;
            break;
        default:
            fatal("Unknown interference type");
    }
}

void
Bus::addInterferenceCycles(int victimID, Tick delay, interference_type iType){
	fatal("deprecated");
}

double
Bus::getActualUtilization(){

	Tick period = curTick - lastSampleTick;

	double avgServiceLat = (double) ((double) serviceCyclesSample / (double) requestSample);
	double busSlots = (double) period / avgServiceLat;
	double actualUtil = (double) requestSample / busSlots;

	serviceCyclesSample = 0;
	requestSample = 0;
	lastSampleTick = curTick;

	return actualUtil;
}

std::vector<int>
Bus::getPerCoreBusAccesses(){
	vector<int> retval = requestPerCoreSample;
	for(int i=0;i<requestPerCoreSample.size();i++) requestPerCoreSample[i] = 0;
	return retval;
}

std::vector<int>
Bus::getPerCoreBusReads(){
	vector<int> retval = readsPerCoreSample;
	for(int i=0;i<readsPerCoreSample.size();i++) readsPerCoreSample[i] = 0;
	return retval;
}

void
Bus::registerTrafficGenerator(TrafficGenerator* _trafGen){
	trafficGenerator = _trafGen;
}

void
Bus::sendGeneratedRequest(MemReqPtr& req){
    DPRINTF(Bus, "Inserting generated request for addr %d\n", req->paddr);
	memoryController->insertRequest(req);

	if (!memoryControllerEvent->scheduled()) {
		memoryControllerEvent->schedule(curTick);
	}
}


#ifdef INJECT_TEST_REQUESTS
void
Bus::generateRequests(){

    // TEST 1: Simple read and write page hits tests
    int numTests = 10;
    Addr address = 0x1000000;
    bool wb = false;

    for(int i=0;i<numTests;i++){
        MemReqPtr tmp = new MemReq();
        tmp->paddr = address;
        tmp->isDDRTestReq = true;
        if(wb) tmp->cmd = Writeback;
        else tmp->cmd = Read;
        wb = !wb;
        testRequests.push_back(tmp);
    }

    // TEST 2: Read and write page hit bursts
    numTests = 10;
    address = 0x1000000;
    int reads = 5;
    int writes = 5;

    for(int i=0;i<numTests;i++){

        for(int j=0;j<reads;j++){
            MemReqPtr tmp = new MemReq();
            tmp->paddr = address;
            tmp->isDDRTestReq = true;
            tmp->cmd = Read;
            testRequests.push_back(tmp);
        }

        for(int j=0;j<writes;j++){
            MemReqPtr tmp = new MemReq();
            tmp->paddr = address;
            tmp->isDDRTestReq = true;
            tmp->cmd = Writeback;
            testRequests.push_back(tmp);
        }
    }

    // TEST 3: Page conflicts
    numTests = 10;
    Addr pageA = 0x2000000;
    Addr pageB = 0x3000000;
    bool usePageA = true;

    for(int i=0;i<numTests;i++){
        MemReqPtr tmp = new MemReq();
        tmp->isDDRTestReq = true;
        tmp->cmd = Read;

        if(usePageA) tmp->paddr = pageA;
        else tmp->paddr = pageB;
        usePageA = !usePageA;

        testRequests.push_back(tmp);
    }


    // TEST 4: Overlapped read and write page accesses
    // memory controller closes pages between reads and writes
    numTests = 10;
    int numReads = 8;
    int numWrites = 8;
    address = 0x1000000;
    int pagesize = 10;

    for(int i=0;i<numTests;i++){

        for(int j=0;j<numReads;j++){

            Addr curAddr = address | (j << pagesize);

            MemReqPtr tmp = new MemReq();
            tmp->paddr = curAddr;
            tmp->isDDRTestReq = true;
            tmp->cmd = Read;
            testRequests.push_back(tmp);
        }

        for(int j=0;j<numWrites;j++){

            Addr curAddr = address | (j << pagesize);

            MemReqPtr tmp = new MemReq();
            tmp->paddr = curAddr;
            tmp->isDDRTestReq = true;
            tmp->cmd = Writeback;
            testRequests.push_back(tmp);
        }
    }

#ifdef DO_BUS_TRACE
    list<MemReqPtr>::iterator it;
    for(it = testRequests.begin(); it != testRequests.end(); it++){
        MemReqPtr tmp = *it;
        writeTraceFileLine(tmp->paddr,
                          (tmp->paddr >> 10) % 8,
                          (tmp->paddr >> 10),
                          -1,
                           tmp->cmd,
                          "Request");
    }
#endif
}
#endif

#ifdef DO_BUS_TRACE
void
Bus::writeTraceFileLine(Addr address, int bank, Addr page, Tick latency, MemCmd cmd, std::string message){
    ofstream file("busAccessTrace.txt", ofstream::app);
    file << curTick << ","
         << address << ","
         << bank << ","
         << page << ","
         << latency << ","
         << cmd << ","
         << message << "\n";
    file.flush();
    file.close();
}
#endif

// Event implementations
void
AddrArbiterEvent::process()
{
    assert(bus->lastarbevent <= this->original_time);
    assert(this->original_time <= curTick);
    bus->lastarbevent = this->original_time;

    if (bus->memoryController->isBlocked()) {
        this->setpriority(Resched_Arb_Pri);
        if(bus->nextfree <= curTick){
            this->schedule(curTick);
            arbitrationLoopCounter++;
            if(arbitrationLoopCounter > MAX_ARB_RESCHED) fatal("Infinite loop of arbitration events");
        } else {
            this->schedule(bus->nextfree);
            arbitrationLoopCounter = 0;
        }
    } else {
        arbitrationLoopCounter = 0;
        bus->arbitrateAddrBus(interfaceid, this->original_time);
        if (!bus->arb_events.empty()) {
            if (bus->need_to_sort) {
                bus->arb_events.sort(event_compare());
                bus->need_to_sort = false;
            }

            // Find next arb event and schedule it.
            AddrArbiterEvent *tmp = bus->arb_events.front();
            if (curTick > tmp->original_time) {
                tmp->schedule(curTick);
            } else {
                assert(tmp->original_time >= curTick);
                tmp->schedule(tmp->original_time);
            }
            bus->arb_events.pop_front();
            bus->currently_scheduled = tmp;
        } else {
            bus->arbiter_scheduled_flag = false;
        }
        delete this;
    }
}

const char *
AddrArbiterEvent::description()
{
    return "address bus arbitration";
}


void
DataArbiterEvent::process()
{
    DPRINTF(Bus, "Data Arb processing event\n");
    bus->arbitrateDataBus();
}

const char *
DataArbiterEvent::description()
{
    return "data bus arbitration";
}


void
DeliverEvent::process()
{
    bi->deliver(req);
    delete this;
}

const char *
DeliverEvent::description()
{
    return "bus deliver";
}

void
MemoryControllerEvent::process()
{
    bus->handleMemoryController(isShadow, controllerID);
}

const char *
MemoryControllerEvent::description()
{
    return "memory controller invocation";
}

// Unused methods

void
Bus::delayData(int size, int senderID, MemCmdEnum cmd)
{
    fatal("delayData() not used in new bus implementation");
}

void
Bus::sendData(MemReqPtr &req, Tick origReqTime)
{
    fatal("sendData not used in new bus implementation");
}

void
Bus::sendAck(MemReqPtr &req, Tick origReqTime)
{
    fatal("sendAck not used in new bus implementation");
}

void
Bus::clearBlocked(int id)
{
    fatal("clearBlocked is not used in new bus implementation");
}

void
Bus::setBlocked(int id)
{
    fatal("setBlocked is not used in the new bus implementation");
}

bool
Bus::findOldestRequest(std::vector<BusRequestRecord> & requests,
                       int & grant_id, int & old_grant_id)
{
    fatal("findOldestRequest is not used in the new bus implementation");
    return false;
}

void
Bus::scheduleArbitrationEvent(Event * arbiterEvent, Tick reqTime,
                              Tick nextFreeCycle, Tick idleAdvance)
{
    fatal("scheduleArbitrationEvent is not used in the new bus implementation");
}

Tick
Bus::probe(MemReqPtr &req, bool update)
{
    fatal("probe is not used in the new bus implementation");
    return 0;
}


#ifndef DOXYGEN_SHOULD_SKIP_THIS

BEGIN_DECLARE_SIM_OBJECT_PARAMS(Bus)

    Param<int> width;
    Param<int> clock;
    SimObjectParam<HierParams *> hier;
    SimObjectParam<AdaptiveMHA *> adaptive_mha;
    Param<int> cpu_count;
    Param<int> bank_count;
    Param<Tick> switch_at;
    SimObjectParam<TimingMemoryController *> fast_forward_controller;
    SimObjectParam<TimingMemoryController *> memory_controller;
    Param<bool> infinite_bw;
    SimObjectParam<InterferenceManager* > interference_manager;
    Param<double> utilization_limit;

END_DECLARE_SIM_OBJECT_PARAMS(Bus)


BEGIN_INIT_SIM_OBJECT_PARAMS(Bus)

    INIT_PARAM(width, "bus width in bytes"),
    INIT_PARAM(clock, "bus frequency"),
    INIT_PARAM_DFLT(hier,
		    "Hierarchy global variables",
		    &defaultHierParams),
    INIT_PARAM_DFLT(adaptive_mha, "Adaptive MHA object",NULL),
    INIT_PARAM(cpu_count, "Number of CPUs"),
    INIT_PARAM(bank_count, "Number of L2 cache banks"),
    INIT_PARAM_DFLT(switch_at, "Tick where memorycontroller is switched", 0),
    INIT_PARAM_DFLT(fast_forward_controller, "Memory controller object used in fastforward", NULL),
    INIT_PARAM_DFLT(memory_controller, "Memory controller object", NULL),
    INIT_PARAM_DFLT(infinite_bw, "Infinite bandwidth and only page hits", false),
    INIT_PARAM_DFLT(interference_manager, "Interference manager", NULL),
    INIT_PARAM_DFLT(utilization_limit, "Data bus utilization limit (single core only)", 0.0)

END_INIT_SIM_OBJECT_PARAMS(Bus)


CREATE_SIM_OBJECT(Bus)
{
    return new Bus(getInstanceName(),
                   hier,
                   width,
                   clock,
                   adaptive_mha,
                   cpu_count,
                   bank_count,
                   switch_at,
                   fast_forward_controller,
                   memory_controller,
                   infinite_bw,
                   interference_manager,
                   utilization_limit);
}

REGISTER_SIM_OBJECT("Bus", Bus)

#endif //DOXYGEN_SHOULD_SKIP_THIS
