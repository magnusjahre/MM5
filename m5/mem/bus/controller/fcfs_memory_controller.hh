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
    
    private:
        int queueLength;
        MemReqPtr pageCmd;
        bool prevActivate;
        
        Addr activePage;
        bool pageActivated;
        
        std::list<Addr> takeOverActiveList;
    
    public:
        std::list<MemReqPtr> memoryRequestQueue; 
        
        /** Constructs a Memory Controller object. */
        FCFSTimingMemoryController(std::string _name, int _queueLength);
    
        /** Frees locally allocated memory. */
        ~FCFSTimingMemoryController();
    
        int insertRequest(MemReqPtr &req);
    
        bool hasMoreRequests();
    
        MemReqPtr& getRequest();
        
        virtual void setOpenPages(std::list<Addr> pages);

};

