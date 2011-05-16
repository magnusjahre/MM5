
#include "mem/bus/controller/memory_controller.hh"

/**
 * A Memory controller.
 */
class FixedBandwidthMemoryController : public TimingMemoryController
{

    private:
        int queueLength;
        int cpuCount;

        std::vector<Addr> activePages;
        std::vector<Tick> activatedAt;
        int numActivePages;

        std::vector<std::vector<MemReqPtr > > perCoreReads;
        std::vector<MemReqPtr> otherRequests;

        void checkBlockingStatus();

    public:

        /** Constructs a Memory Controller object. */
        FixedBandwidthMemoryController(std::string _name, int _queueLength, int _cpuCount);

        /** Frees locally allocated memory. */
        ~FixedBandwidthMemoryController();

        int insertRequest(MemReqPtr &req);

        bool hasMoreRequests();

        MemReqPtr getRequest();

        virtual void setOpenPages(std::list<Addr> pages);

        virtual void computeInterference(MemReqPtr& req, Tick busOccupiedFor);

};

