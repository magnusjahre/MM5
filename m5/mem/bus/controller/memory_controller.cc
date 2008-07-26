/**
 * @file
 * Describes the Memory Controller API.
 */


#include "mem/bus/controller/memory_controller.hh"

using namespace std;

TimingMemoryController::TimingMemoryController(std::string _name)
    : SimObject(_name) {
  isBlockedFlag = false;
  isPrewriteBlockedFlag = false;
}

/** Frees locally allocated memory. */
TimingMemoryController::~TimingMemoryController(){
}

void
TimingMemoryController::registerBus(Bus* _bus, int cpuCount){ 
    bus = _bus; 
    memCtrCPUCount = cpuCount;
    cpuLastActivated.resize(cpuCount, std::vector<Addr>(_bus->getBankCount(), 0));
    lastActivated.resize(_bus->getBankCount(), 0);
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
TimingMemoryController::getPageAddr(Addr addr)
{
    return (addr << mem_interface->getPageSize());
}

void
TimingMemoryController::currentActivationAddress(int cpuID, Addr addr, int bank){
    lastActivated[bank] = addr;
    //FIXME: implement updating perCPU activation addrs in cpuLastActivated
}

bool 
TimingMemoryController::isPageHit(Addr addr, int bank){
    return addr != lastActivated[bank];
}

#ifndef DOXYGEN_SHOULD_SKIP_THIS

DEFINE_SIM_OBJECT_CLASS_NAME("TimingMemoryController", TimingMemoryController);

#endif


