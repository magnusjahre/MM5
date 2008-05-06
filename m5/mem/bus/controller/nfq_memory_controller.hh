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
        int nfqNumCPUs;
        
        int processorPriority;
        int processorInc;
        int writebackPriority;
        int writebackInc;
        
        std::vector<Tick> virtualFinishTimes;
        std::vector<std::vector<MemReqPtr> > requests;
        
        std::vector<Addr> activePages;
        
        MemReqPtr pageCmd;
    
        int queuedReads;
        int queuedWrites;
        
        Tick getMinStartTag();
    
    public:
        
        /** Constructs a Memory Controller object. */
        NFQMemoryController(std::string _name,
                            int _rdQueueLength,
                            int _wrQueueLength,
                            int _spt,
                            int _numCPUs,
                            int _processorPriority,
                            int _writePriority);
    
        /** Frees locally allocated memory. */
        ~NFQMemoryController();
    
        int insertRequest(MemReqPtr &req);
    
        bool hasMoreRequests();
    
        MemReqPtr& getRequest();

};

