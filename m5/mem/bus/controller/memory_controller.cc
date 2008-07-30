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
    //FIXME: number of banks is hardcoded
    cpuLastActivated.resize(cpuCount, std::vector<Addr>(8, 0));
    lastActivated.resize(8, 0);
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
    
    //FIXME: number of banks is hardcoded
    
    assert(bank >= 0 && bank < lastActivated.size());
    lastActivated[bank] = addr;
    if(cpuID >= 0){
        assert(cpuID >=  0 && cpuID < cpuLastActivated.size());
        assert(cpuLastActivated[0].size() == 8);
        assert(bank >= 0 && bank < cpuLastActivated[0].size());
        cpuLastActivated[cpuID][bank] = addr;
    }
}

bool 
TimingMemoryController::isPageHit(Addr addr, int bank){
    
    //FIXME: number of banks is hardcoded
    
    assert(bank >= 0 && bank < lastActivated.size());
    return addr != lastActivated[bank];
}

bool
TimingMemoryController::isPageHitOnPrivateSystem(Addr addr, int bank, int cpuID){
    
    //FIXME: number of banks is hardcoded
    
    if(cpuID == -1) return false;
    assert(cpuID >=  0 && cpuID < cpuLastActivated.size());
    assert(cpuLastActivated[0].size() == 8);
    assert(bank >= 0 && bank < cpuLastActivated[0].size());
    return addr != cpuLastActivated[cpuID][bank];
}

#ifndef DOXYGEN_SHOULD_SKIP_THIS

DEFINE_SIM_OBJECT_CLASS_NAME("TimingMemoryController", TimingMemoryController);

#endif


