/**
 * @file
 * FCFS memory controller
 *
 */

#include "mem/bus/controller/memory_controller.hh"
#include "mem/requesttrace.hh"

using namespace std;
/**
 * A Memory controller.
 */
class RDFCFSTimingMemoryController : public TimingMemoryController
{
  private:

    std::list<MemReqPtr> readQueue;
    std::list<MemReqPtr> writeQueue;
    MemReqPtr lastIssuedReq;
    bool lastIsWrite;
    bool infiniteWriteBW;

    Tick currentDeliveredReqAt;
    int currentOccupyingCPUID;
    Tick lastDeliveredReqAt;
    int lastOccupyingCPUID;

    std::list<MemReqPtr>::iterator queueIterator;

    struct ActivationEntry{
        Addr address;
        Tick activatedAt;

        ActivationEntry(Addr a, Tick aAt){
            address = a;
            activatedAt = aAt;
        }

        ActivationEntry(){
            address = 0;
            activatedAt = 0;
        }
    };

    std::map<int,ActivationEntry> activePages;
    std::map<int,ActivationEntry>::iterator pageIterator;

    int num_active_pages;

    int readqueue_size;
    int writequeue_size;
    int reserved_slots;

    bool getActivate(MemReqPtr& req);
    bool getClose(MemReqPtr& req);
    bool getReady(MemReqPtr& req);
    bool getOther(MemReqPtr& req);

    bool equalReadWritePri;
    bool closedPagePolicy;

    int starvationPreventionThreshold;
    int numReqsPastOldest;

    std::list<MemReqPtr> mergeQueues();
    bool closePageForRequest(MemReqPtr& choosenReq, MemReqPtr& oldestReq);

    std::vector<Tick> requestSequenceNumbers;

    RequestTrace aloneAccessOrderTraces;
    std::vector<RequestTrace> pageResultTraces;

  public:

    typedef enum{
        FCFS,
        RoW
    } priority_scheme;

    typedef enum{
        OPEN_PAGE,
        CLOSED_PAGE
    } page_policy;

    // constructor
    /** Constructs a Memory Controller object. */
    RDFCFSTimingMemoryController(std::string _name,
                                 int _readqueue_size,
                                 int _writequeue_size,
                                 int _reserved_slots,
                                 bool _infinite_write_bw,
                                 priority_scheme _priority_scheme,
                                 page_policy _page_policy);

    /** Frees locally allocated memory. */
    ~RDFCFSTimingMemoryController();

    int insertRequest(MemReqPtr &req);

    bool hasMoreRequests();

    MemReqPtr getRequest();

    virtual std::list<Addr> getOpenPages(){
        std::list<Addr> retlist;

        for(pageIterator=activePages.begin();pageIterator!=activePages.end();pageIterator++){
            ActivationEntry tmp = pageIterator->second;
            retlist.push_back(tmp.address);
        }

        return retlist;
    }

    virtual std::list<MemReqPtr>  getPendingRequests();

    virtual void setOpenPages(std::list<Addr> pages);

//     virtual void addInterference(MemReqPtr &req, Tick lat);

    virtual int getReadQueueLength(){
        return readqueue_size;
    }

    virtual int getWriteQueueLength(){
        return writequeue_size;
    }

    virtual int getWaitingReadCount(){
        return readQueue.size();
    }

    virtual int getWaitingWriteCount(){
        return writeQueue.size();
    }

    virtual void computeInterference(MemReqPtr& req, Tick busOccupiedFor);

    virtual void initializeTraceFiles(Bus* regbus);

};
