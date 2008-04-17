/**
 * @file
 * FCFS memory controller 
 *
 */

#include "mem/bus/memory_controller.hh"


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

    std::list<MemReqPtr>::iterator queueIterator;

    std::list<Addr> activePages;
    std::list<Addr>::iterator pageIterator;

    int num_active_pages;
    int max_active_pages;

  public:

    // Memory Request currently being issued
    MemReqPtr activate;
    MemReqPtr close;

    // constructor
    /** Constructs a Memory Controller object. */
    RDFCFSTimingMemoryController();

    /** Frees locally allocated memory. */
    ~RDFCFSTimingMemoryController();

    int insertRequest(MemReqPtr &req);

    bool hasMoreRequests();

    MemReqPtr& getRequest();

};
