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
}

/** Frees locally allocated memory. */
TimingMemoryController::~TimingMemoryController(){
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



#ifndef DOXYGEN_SHOULD_SKIP_THIS

DEFINE_SIM_OBJECT_CLASS_NAME("TimingMemoryController", TimingMemoryController);

#endif


