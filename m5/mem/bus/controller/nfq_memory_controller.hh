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

        std::vector<int> processorIncrements;

        std::vector<Tick> virtualFinishTimes;
        std::vector<std::vector<MemReqPtr> > requests;

        std::vector<Addr> activePages;

        MemReqPtr pageCmd;

        int queuedReads;
        int queuedWrites;

        int starvationCounter;

        bool infiniteWriteBW;

    private:
        Tick getMinStartTag();

        bool findColumnRequest(MemReqPtr& req, bool (NFQMemoryController::*compare)(MemReqPtr&));

        bool findRowRequest(MemReqPtr& req);

        bool pageActivated(MemReqPtr& req);

        MemReqPtr& createCloseReq(Addr pageAddr);

        MemReqPtr& createActivateReq(MemReqPtr& req);

        MemReqPtr& prepareColumnRequest(MemReqPtr& req);

        void printRequestQueue(Tick fromTick);

        void setUpWeights(std::vector<double> priorities);

        int getQueueID(int cpuID);

    public:

        /** Constructs a Memory Controller object. */
        NFQMemoryController(std::string _name,
                            int _rdQueueLength,
                            int _wrQueueLength,
                            int _spt,
                            int _numCPUs,
                            std::vector<double> priorities,
                            bool _infiniteWriteBW);

        /** Frees locally allocated memory. */
        ~NFQMemoryController();

        int insertRequest(MemReqPtr &req);

        bool hasMoreRequests();

        MemReqPtr getRequest();

        virtual void setOpenPages(std::list<Addr> pages);

        virtual void computeInterference(MemReqPtr& req, Tick busOccupiedFor){
        	// not needed
        }

        virtual void setBandwidthQuotas(std::vector<double> quotas){
        	setUpWeights(quotas);
        }
};

