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

        std::vector<Addr> activePages;
        std::vector<Tick> activatedAt;
        int numActivePages;

        std::list<MemReqPtr> memoryRequestQueue;

    public:

        /** Constructs a Memory Controller object. */
        FCFSTimingMemoryController(std::string _name, int _queueLength);

        /** Frees locally allocated memory. */
        ~FCFSTimingMemoryController();

        int insertRequest(MemReqPtr &req);

        bool hasMoreRequests();

        MemReqPtr getRequest();

        virtual void setOpenPages(std::list<Addr> pages);

        virtual void computeInterference(MemReqPtr& req, Tick busOccupiedFor);

};

