/**
 * @file
 * Describes the Memory Controller API.
 */


#include "mem/bus/controller/memory_controller.hh"

#include "mem/cache/cache.hh"
#include "mem/cache/tags/lru.hh"
#include "base/compression/null_compression.hh"
#include "mem/cache/miss/miss_queue.hh"
#include "mem/cache/coherence/uni_coherence.hh"

#include "mem/bus/bus.hh"
#include "mem/bus/slave_interface.hh"
#include "base/compression/null_compression.hh"
#include "mem/timing/simple_mem_bank.hh"

using namespace std;

TimingMemoryController::TimingMemoryController(std::string _name)
: SimObject(_name) {
	isBlockedFlag = false;
	isPrewriteBlockedFlag = false;
	isShadow = false;
	bus = NULL;
	mem_interface = NULL;
	controllerInterference = NULL;

	max_active_pages = 4;
	memCtrCPUCount = -1;
}

/** Frees locally allocated memory. */
TimingMemoryController::~TimingMemoryController(){
}

void
TimingMemoryController::regStats(){

	assert(memCtrCPUCount != -1);

	sumPrivateQueueLength
		.init(memCtrCPUCount)
		.name(name() + ".sum_private_queue_lengths")
		.desc("sum of the estimated number of reqs a had to wait for");

	numRequests
		.init(memCtrCPUCount)
		.name(name() + ".number_of_requests")
		.desc("number of requests ");

	avgPrivateQueueLength
		.name(name() + ".avg_private_queue_length")
		.desc("average number of requests a request has to wait for");

	avgPrivateQueueLength = sumPrivateQueueLength / numRequests;

}

void
TimingMemoryController::registerBus(Bus* _bus, int cpuCount){
    bus = _bus;
    memCtrCPUCount = cpuCount;
    initializeTraceFiles(_bus);
}

void
TimingMemoryController::registerInterface(BaseInterface *interface){
	mem_interface = interface;
}

void
TimingMemoryController::setBlocked()
{
    assert(!isBlockedFlag);
    startBlocking = curTick;
    isBlockedFlag = true;
    DPRINTF(Blocking, "Blocking the Memory Controller\n");
}

void
TimingMemoryController::setUnBlocked()
{
    assert(isBlockedFlag);
    totalBlocktime += (curTick - startBlocking);
    bus->incrementBlockedCycles(curTick - startBlocking);
    isBlockedFlag = false;
    DPRINTF(Blocking, "Unblocking the Memory Controller\n");
}

void
TimingMemoryController::setPrewriteBlocked()
{
  assert(!isPrewriteBlockedFlag);
  isPrewriteBlockedFlag = true;
}

void
TimingMemoryController::setPrewriteUnBlocked()
{
  assert(isPrewriteBlockedFlag);
  isPrewriteBlockedFlag = false;
}

Addr
TimingMemoryController::getPage(MemReqPtr &req)
{
    return (req->paddr >> mem_interface->getPageSize());
}

Addr
TimingMemoryController::getPage(Addr addr)
{
    return (addr >> mem_interface->getPageSize());
}


Addr
TimingMemoryController::getPageAddr(Addr addr)
{
    return (addr << mem_interface->getPageSize());
}

void
TimingMemoryController::insertPrivateVirtualRequest(MemReqPtr& req){
	controllerInterference->insertPrivateVirtualRequest(req);
}

bool
TimingMemoryController::addsInterference(){
    return controllerInterference->addsInterference();
}

void
TimingMemoryController::addBusInterference(Tick service, Tick queue, MemReqPtr& req, int forCPU){
    return bus->addBusInterference(service, queue, req, forCPU);
}



#ifndef DOXYGEN_SHOULD_SKIP_THIS

DEFINE_SIM_OBJECT_CLASS_NAME("TimingMemoryController", TimingMemoryController);

#endif


