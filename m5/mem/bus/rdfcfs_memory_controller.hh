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
    std::list<MemReqPtr> prewritebackQueue; 

    std::list<MemReqPtr> workQueue; 

    std::list<MemReqPtr>::iterator queueIterator;

    std::list<Addr> activePages;
    std::list<Addr>::iterator pageIterator;

    int num_active_pages;
    int max_active_pages;

    Tick total_read_wait;
    Tick total_write_wait;
    Tick total_prewrite_wait;

    int avg_read_size;
    int avg_write_size;
    int avg_prewrite_size;

    Tick last_invoke;
    Tick last_invoke_internal;

    int reads;
    int writes;
    int prewrites;

    bool invoked;

  public:

    // Memory Request currently being issued
    MemReqPtr activate;
    MemReqPtr close;

    MemReqPtr current;

    // constructor
    /** Constructs a Memory Controller object. */
    RDFCFSTimingMemoryController();

    /** Frees locally allocated memory. */
    ~RDFCFSTimingMemoryController();

    int insertRequest(MemReqPtr &req);

    bool hasMoreRequests();

    MemReqPtr& getRequest();

    std::string dumpstats();

};
