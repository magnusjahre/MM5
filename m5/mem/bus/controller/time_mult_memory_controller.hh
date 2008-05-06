/**
 * @file
 * FCFS memory controller 
 *
 */

#include "mem/bus/controller/memory_controller.hh"

/**
 * A Memory controller.
 */
class TimeMultiplexedMemoryController : public TimingMemoryController
{
    
    private:
        int queueLength;
        MemReqPtr pageCmd;
        bool prevActivate;
        
        Addr activePage;
        bool pageActivated;
    
    public:
        std::list<MemReqPtr> memoryRequestQueue; 
        
        /** Constructs a Memory Controller object. */
        TimeMultiplexedMemoryController(std::string _name, int _queueLength);
    
        /** Frees locally allocated memory. */
        ~TimeMultiplexedMemoryController();
    
        int insertRequest(MemReqPtr &req);
    
        bool hasMoreRequests();
    
        MemReqPtr& getRequest();

};

