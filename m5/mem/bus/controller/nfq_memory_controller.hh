/**
 * @file
 * FCFS memory controller 
 *
 */

#include "mem/bus/controller/memory_controller.hh"

/**
 * A Memory controller.
 */
class NFQMemoryController : public TimingMemoryController
{
    
    private:
        int readQueueLength;
        int writeQueueLenght;
        int starvationPreventionThreshold;
        
        MemReqPtr pageCmd;

    
    public:
        std::list<MemReqPtr> memoryRequestQueue; 
        
        /** Constructs a Memory Controller object. */
        NFQMemoryController(std::string _name,
                            int _rdQueueLength,
                            int _wrQueueLength,
                            int _spt);
    
        /** Frees locally allocated memory. */
        ~NFQMemoryController();
    
        int insertRequest(MemReqPtr &req);
    
        bool hasMoreRequests();
    
        MemReqPtr& getRequest();

};

