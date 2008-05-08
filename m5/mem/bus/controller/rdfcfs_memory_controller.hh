/**
 * @file
 * FCFS memory controller 
 *
 */

#include "mem/bus/controller/memory_controller.hh"


using namespace std;
/**
 * A Memory controller.
 */
class RDFCFSTimingMemoryController : public TimingMemoryController
{
  private:
    std::list<MemReqPtr> readQueue; 
    std::list<MemReqPtr> writeQueue;
    MemReqPtr lastIssuedReq;
    bool lastIsWrite;

    std::list<MemReqPtr>::iterator queueIterator;

    std::list<Addr> activePages;
    std::list<Addr>::iterator pageIterator;

    int num_active_pages;
    int max_active_pages;
    
    int readqueue_size;
    int writequeue_size;
    int reserved_slots;

    bool getActivate(MemReqPtr& req);
    bool getClose(MemReqPtr& req);
    bool getReady(MemReqPtr& req);
    bool getOther(MemReqPtr& req);
    
  public:

    // Memory Request currently being issued
    MemReqPtr activate;
    MemReqPtr close;

    // constructor
    /** Constructs a Memory Controller object. */
    RDFCFSTimingMemoryController(std::string _name,
                                 int _readqueue_size,
                                 int _writequeue_size,
                                 int _reserved_slots);

    /** Frees locally allocated memory. */
    ~RDFCFSTimingMemoryController();

    int insertRequest(MemReqPtr &req);

    bool hasMoreRequests();

    MemReqPtr& getRequest();
    
    virtual std::list<Addr> getOpenPages(){
        return activePages;
    }
    
    virtual std::list<MemReqPtr>  getPendingRequests();
    
    virtual void setOpenPages(std::list<Addr> pages);

};
