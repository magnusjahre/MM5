/**
 * @file
 * FCFS memory controller 
 *
 */

#include "mem/bus/memory_controller.hh"

/**
 * A Memory controller.
 */
class FCFSPRITimingMemoryController : public TimingMemoryController
{
  private:
    bool startedprewrite;
  public:
    std::list<MemReqPtr> memoryRequestQueue; 
    std::list<MemReqPtr> prewritebackQueue; 
    
    // constructor
    /** Constructs a Memory Controller object. */
    FCFSPRITimingMemoryController();

    /** Frees locally allocated memory. */
    ~FCFSPRITimingMemoryController();

    int insertRequest(MemReqPtr &req);

    bool hasMoreRequests();

    MemReqPtr& getRequest();

};
