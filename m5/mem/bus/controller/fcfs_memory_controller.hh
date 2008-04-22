/**
 * @file
 * FCFS memory controller 
 *
 */

#include "mem/bus/controller/memory_controller.hh"

/**
 * A Memory controller.
 */
class FCFSTimingMemoryController : public TimingMemoryController
{
  public:
    std::list<MemReqPtr> memoryRequestQueue; 
    
    // constructor
    /** Constructs a Memory Controller object. */
    FCFSTimingMemoryController(std::string _name, int _queueLength);

    /** Frees locally allocated memory. */
    ~FCFSTimingMemoryController();

    int insertRequest(MemReqPtr &req);

    bool hasMoreRequests();

    MemReqPtr& getRequest();

};

