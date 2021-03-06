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
#include "mem/bus/base_interface.hh"
#include "mem/bus/bus.hh"
#include "mem/bus/controller/controller_interference.hh"

class Bus;
class ControllerInterference;

/**
 * A Memory controller.
 */
class TimingMemoryController : public SimObject
{
  private:
    Tick totalBlocktime;

  protected:

    Bus* bus;
    int memCtrCPUCount;

    bool isBlockedFlag;
    Tick startBlocking;
    bool isPrewriteBlockedFlag;
    BaseInterface *mem_interface;

    bool isShadow;

    int max_active_pages;

    ControllerInterference* controllerInterference;

    Stats::Scalar<> pageHits;
    Stats::Formula pageHitRate;

    Stats::Scalar<> pageMisses;
    Stats::Formula pageMissRate;

    Stats::Scalar<> sentRequests;

    Stats::Vector<> sumPrivateQueueLength;
    Stats::Vector<> numRequests;
    Stats::Formula avgPrivateQueueLength;

  public:
    // constructor
    /** Constructs a Memory Controller object. */
    TimingMemoryController(std::string _name);

    /** Frees locally allocated memory. */
    virtual ~TimingMemoryController();

    void regStats();

    void registerBus(Bus* _bus, int cpuCount);

    virtual int insertRequest(MemReqPtr &req) = 0;

    virtual bool hasMoreRequests() = 0;

    BaseInterface* getMemoryInterface(){
    	assert(mem_interface != NULL);
    	return mem_interface;
    }

    virtual MemReqPtr getRequest(){
        fatal("not implemented");
    }

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
    Addr getPage(Addr req);
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

    void registerInterface(BaseInterface *interface);

    void registerInterferenceMeasurement(ControllerInterference* contInt){
    	assert(controllerInterference == NULL);
    	controllerInterference = contInt;
    }

    int getMemoryBankID(Addr addr){
        return mem_interface->getMemoryBankID(addr);
    }

    Tick getBankActivatedAt(int bankID){
        return mem_interface->getBankActivatedAt(bankID);
    }

    void setShadow(){
        isShadow = true;
    }

    virtual int getReadQueueLength(){
        return 0;
    }

    virtual int getWriteQueueLength(){
        return 0;
    }

    virtual int getWaitingReadCount(){
        return 0;
    }

    virtual int getWaitingWriteCount(){
        return 0;
    }

    virtual void computeInterference(MemReqPtr& req, Tick busOccupiedFor){
        fatal("computeInterferenece() in TimingMemoryController does not make sense");
    }

    virtual void initializeTraceFiles(Bus* regbus){
        // does nothing unless impl in a subclass
    }

    virtual int getMaxActivePages(){
    	return max_active_pages;
    }

    void insertPrivateVirtualRequest(MemReqPtr& req);

    virtual void setBandwidthQuotas(std::vector<double> quotas){
    	fatal("Controller does not support bandwidth quotas");
    }

    virtual void lastRequestLatency(int cpuID, int latency){

    }

    bool addsInterference();

    void addBusQueueInterference(Tick interference, MemReqPtr& req);
    void addBusServiceInterference(Tick interference, MemReqPtr& req);

    virtual void setASRHighPriCPUID(int cpuID){

    }

    virtual void incrementWaitRequestCnt(int increment){
    	fatal("Memory controller does not support incrementWaitRequestCnt()");
    }

    void checkMaxActivePages();

};

#endif // __TIMINGMEMORYCONTROLLER_HH__
