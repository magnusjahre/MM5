/**
 * @file
 * Describes the Memory Controller API.
 */

#ifndef __TIMINGMEMORYCONTROLLER_HH__
#define __TIMINGMEMORYCONTROLLER_HH__

#include <vector>
#include <list>
#include <string>

#include "sim/sim_object.hh"
#include "sim/builder.hh"
#include "mem/mem_req.hh"
// #include "mem/base_hier.hh"
// #include "base/statistics.hh"
// #include "base/range.hh"
// #include "sim/eventq.hh"
#include "mem/bus/base_interface.hh"
#include "mem/bus/bus.hh"

class Bus;

/**
 * A Memory controller.
 */
class TimingMemoryController : public SimObject
{
  private:
    Tick totalBlocktime;
    
    std::vector<std::vector<Addr> > cpuLastActivated;
    std::vector<Addr> lastActivated;
    std::vector<Addr> lastActivatedBy;
    
  protected:
      
    Bus* bus;
    int memCtrCPUCount;
      
    bool isBlockedFlag;
    Tick startBlocking;
    bool isPrewriteBlockedFlag;
    BaseInterface *mem_interface;
    
    bool isShadow;
    
    Stats::Scalar<> pageHits;
    Stats::Formula pageHitRate;
    
    Stats::Scalar<> pageMisses;
    Stats::Formula pageMissRate;
    
    Stats::Scalar<> sentRequests;
    
    
  public:
    // constructor
    /** Constructs a Memory Controller object. */
    TimingMemoryController(std::string _name);

    /** Frees locally allocated memory. */
    virtual ~TimingMemoryController();

    void registerBus(Bus* _bus, int cpuCount);
    
    virtual int insertRequest(MemReqPtr &req) = 0;

    virtual bool hasMoreRequests() = 0;

    virtual MemReqPtr& getRequest() = 0;
    
    virtual std::list<Addr> getOpenPages(){
        fatal("getOpenPages() is not implemented");
    }
    
    virtual std::list<MemReqPtr>  getPendingRequests(){
        fatal("getPendingRequests() is not implemented");
    }
    
    virtual void setOpenPages(std::list<Addr> pages) = 0;

    void setBlocked();

    void setUnBlocked();

    bool isBlocked() {
      return isBlockedFlag;
    }

    void setPrewriteBlocked();

    void setPrewriteUnBlocked();

    bool isPrewriteBlocked() {
      return isPrewriteBlockedFlag;
    }

    // Get the corresponding page to a memory request
    Addr getPage(MemReqPtr &req);
    Addr getPageAddr(Addr addr);

    // Check if the request is contacting an active page
    bool isActive(MemReqPtr &req) {
      return mem_interface->isActive(req);
    }

    // Check to see if the bank corresponding to a request is closed
    bool bankIsClosed(MemReqPtr &req) {
      return mem_interface->bankIsClosed(req);
    }

    // Check to see if request is ready
    bool isReady(MemReqPtr &req) {
      return mem_interface->isReady(req);
    }

    void registerInterface(BaseInterface *interface) {
        mem_interface = interface;
    }
    
    int getMemoryBankID(Addr addr){
        return mem_interface->getMemoryBankID(addr);
    }
    
    void currentActivationAddress(int cpuID, Addr addr, int bank);
    
    bool isPageHit(Addr addr, int bank);
    
    bool isPageHitOnPrivateSystem(Addr addr, int bank, int cpuID);
    
    int getLastActivatedBy(int bank);
    
    virtual void addInterference(MemReqPtr &req, Tick lat){
        fatal("not implemented");
    }
    
    void setShadow(){
        isShadow = true;
    }
};

#endif // __TIMINGMEMORYCONTROLLER_HH__
