
#include "mem/bus/controller/memory_controller.hh"

/**
 * A Memory controller.
 */
class FixedBandwidthMemoryController : public TimingMemoryController
{

    private:
        int queueLength;
        int cpuCount;
        int curSeqNum;

        int starvationThreshold;
        int starvationCounter;

        MemReqPtr invalidRequest;
        MemReqPtr activate;
        MemReqPtr close;

        std::vector<Addr> activePages;

        std::vector<MemReqPtr> requests;

        Tick lastRunAt;
        std::vector<double> tokens;
        std::vector<double> targetAllocation;

        void checkBlockingStatus();
        void addTokens();
        MemReqPtr findHighestPriRequest();
        MemReqPtr findAdminRequests(MemReqPtr& highestPriReq);
        void prepareCloseReq(Addr pageAddr);

        int getQueueID(int reqCPUID){
        	if(reqCPUID != -1) return reqCPUID;
        	return cpuCount;
        }

        bool consideredReady(MemReqPtr& req);
        bool hasMoreTokens(MemReqPtr& req1, MemReqPtr& req2);
        bool hasEqualTokens(MemReqPtr& req1, MemReqPtr& req2);
        bool isOlder(MemReqPtr& req1, MemReqPtr& req2);
        void removeRequest(MemReqPtr& req);

    public:

        /** Constructs a Memory Controller object. */
        FixedBandwidthMemoryController(std::string _name,
        							   int _queueLength,
        							   int _cpuCount,
        							   int _starvationThreshold);

        /** Frees locally allocated memory. */
        ~FixedBandwidthMemoryController();

        int insertRequest(MemReqPtr &req);

        bool hasMoreRequests();

        MemReqPtr getRequest();

        virtual void setOpenPages(std::list<Addr> pages);

        virtual void computeInterference(MemReqPtr& req, Tick busOccupiedFor);

        virtual void lastRequestLatency(int cpuID, int latency);

        virtual void setBandwidthQuotas(std::vector<double> quotas);

};

