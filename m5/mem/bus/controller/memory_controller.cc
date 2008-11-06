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
  isShadow = false;
}

/** Frees locally allocated memory. */
TimingMemoryController::~TimingMemoryController(){
}

void
TimingMemoryController::registerBus(Bus* _bus, int cpuCount){ 
    bus = _bus; 
    memCtrCPUCount = cpuCount;
    
    //FIXME: number of banks is hardcoded
    cpuCurrentActivated.resize(cpuCount, std::vector<Addr>(8, 0));
    cpuCurrentActivatedAt.resize(cpuCount, std::vector<Tick>(8, 0));
    cpuCurrentActivatedFirstUse.resize(cpuCount, std::vector<bool>(8, true));
    currentActivated.resize(8, 0);
    currentActivatedBy.resize(8, 0);
    currentActivatedAt.resize(8, 0);
    currentActivatedFirstUse.resize(8, true);
    
    cpuLastActivated.resize(cpuCount, std::vector<Addr>(8, 0));
    cpuLastActivatedAt.resize(cpuCount, std::vector<Tick>(8, 0));
    lastActivated.resize(8, 0);
    lastActivatedBy.resize(8, 0);
    lastActivatedAt.resize(8, 0);
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
TimingMemoryController::currentActivationAddress(int cpuID, Addr addr, int bank){
    
    cout << curTick << ": Activating new page, bank "<< bank << " page " << getPage(addr) << "\n";
    
    lastActivated[bank] = currentActivated[bank];
    lastActivatedBy[bank] = currentActivatedBy[bank];
    lastActivatedAt[bank] = currentActivatedAt[bank];
    if(cpuID >= 0){
        cpuLastActivated[cpuID][bank] = cpuCurrentActivated[cpuID][bank];
        cpuLastActivatedAt[cpuID][bank] = cpuCurrentActivatedAt[cpuID][bank];
    }
    
    currentActivated[bank] = getPage(addr);
    currentActivatedBy[bank] = cpuID;
    currentActivatedAt[bank] = curTick;
    currentActivatedFirstUse[bank] = true;
    if(cpuID >= 0){
        cpuCurrentActivated[cpuID][bank] = getPage(addr);
        cpuCurrentActivatedAt[cpuID][bank] = curTick;
        cpuCurrentActivatedFirstUse[cpuID][bank] = true;
    }
}

int
TimingMemoryController::getLastActivatedBy(int bank){
    return currentActivatedBy[bank];
}

//NOTE: all hit estimators are idealized models, does not handle closing of pages

bool 
TimingMemoryController::isPageHit(Addr addr, int bank){
    assert(bank >= 0 && bank < currentActivated.size());
    
    if(currentActivatedFirstUse[bank]){
        currentActivatedFirstUse[bank] = false;
        return false;
    }
    
    return getPage(addr) != currentActivated[bank];
}

bool
TimingMemoryController::isPageConflict(MemReqPtr& req){
    assert(req->adaptiveMHASenderID != -1);
    int bank = getMemoryBankID(req->paddr);
    assert(currentActivated[bank] == getPage(req->paddr));
    return getPage(req->paddr) != lastActivated[bank] && lastActivatedAt[bank] >= req->inserted_into_memory_controller;
}

bool
TimingMemoryController::isPageHitOnPrivateSystem(Addr addr, int bank, int cpuID){
    if(cpuID == -1) return false;
    cout << curTick << ": Page hit check, comparing " << getPage(addr) << " and " << cpuCurrentActivated[cpuID][bank] << "\n";
    
    if(cpuCurrentActivatedFirstUse[cpuID][bank]){
        cpuCurrentActivatedFirstUse[cpuID][bank] = false;
        return false;
    }
    
    return getPage(addr) == cpuCurrentActivated[cpuID][bank];
}

bool 
TimingMemoryController::isPageConflictOnPrivateSystem(MemReqPtr& req){
    
    int cpuID = req->adaptiveMHASenderID;
    int bank = getMemoryBankID(req->paddr);
    Addr addr = req->paddr;
    
    if(cpuID == -1) return false;
    
    assert(currentActivated[bank] == getPage(req->paddr));
    return getPage(addr) != cpuLastActivated[cpuID][bank] && cpuLastActivated[cpuID][bank] != 0;
}

#ifndef DOXYGEN_SHOULD_SKIP_THIS

DEFINE_SIM_OBJECT_CLASS_NAME("TimingMemoryController", TimingMemoryController);

#endif


