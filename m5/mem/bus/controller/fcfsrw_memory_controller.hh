/**
 * @file
 * FCFS memory controller 
 *
 */

#include "mem/bus/controller/memory_controller.hh"

/**
 * A Memory controller.
 */
class FCFSRWTimingMemoryController : public TimingMemoryController
{
  public:
    std::list<MemReqPtr> memoryReadQueue; 
    std::list<MemReqPtr> memoryWriteQueue; 

    //The currently open page
    Addr openpage;

    bool pageclosed;

    // Memory Request currently being issued
    MemReqPtr activate;
    MemReqPtr close;

    MemReqPtr current;
    
    // constructor
    /** Constructs a Memory Controller object. */
    FCFSRWTimingMemoryController();

    /** Frees locally allocated memory. */
    ~FCFSRWTimingMemoryController();

    int insertRequest(MemReqPtr &req);

    bool hasMoreRequests();

    MemReqPtr& getRequest();

};

