
#ifndef __DIRECTORY_PROTOCOL_HH__
#define __DIRECTORY_PROTOCOL_HH__

#include "sim/sim_object.hh"
#include "mem/cache/base_cache.hh"
#include "mem/mem_req.hh"
#include "mem/cache/cache_blk.hh"
#include "mem/cache/tags/cache_tags.hh"
        
#define OUTFILENAME "coherencetrace.txt"
        
class BaseCache;
template <class TagStore> class DirectoryProtocolDumpEvent;
        
/**
* This class implements an interface between the directory protocol
* implementation added in this work and the M5 cache implementation. To add 
* more directory implementations a new subclass can be added to this file.
*
* The responsibility of this class is to specify convenience methods that are
* needed for more than one protocol. In the current implementation, this 
* functionality is limited to protocol trace facilities for debugging, 
* coherence message tracing and access to the directory.
*
* @author Magnus Jahre
*/
template <class TagStore>
class DirectoryProtocol
{
    protected:
        
        BaseCache *cache;
        typedef typename TagStore::BlkType BlkType;
        std::string cacheName;
        
        std::string protocol;
        int directoryCpuCount;
        int directoryCpuID;
        
        std::map<Addr, int> blockStore;
        
        bool doTrace;
        int traceStart;
        
        DirectoryProtocolDumpEvent<TagStore>* dumpEvent;
        
        double lastNumRedirectedReads;
        double lastNumOwnerRequests;
        double lastNumOwnerWritebacks;
        double lastNumSharerWritebacks;
        double lastNumNACKs;
        
        std::string dumpFileName;
        
    public:
        
        MemReqList directoryRequests;
        
        // Stats
        Stats::Scalar<> numRedirectedReads;
        Stats::Scalar<> numOwnerRequests;
        Stats::Scalar<> numOwnerWritebacks;
        Stats::Scalar<> numSharerWritebacks;
        Stats::Scalar<> numNACKs;
        
    public:
        
        /**
        * This constructor creates the message trace and message profile if
        * needed.
        *
        * @param _name         The name from the config file.
        * @param _protocol     The name of the protocol that will be used
        * @param _doTrace      If true, the protocol actions are written to a 
        *                      trace file
        * @param _dumpInterval The number of clock cycles between each protocol
        *                      profile event
        * @param _traceStart   The clock cycle to start tracing protocol 
        *                      actions
        */
        DirectoryProtocol(const std::string &_name,
                          const std::string &_protocol,
                          bool _doTrace,
                          int _dumpInterval,
                          int _traceStart);
        
        /**
        * Empty destructor.
        */
        virtual ~DirectoryProtocol(){}
        
        /**
        * This method is called in the cache builder and makes sure the
        * cache knows which cache it is associated with.
        *
        * @param _cache A pointer to the associated cache
        */
        void setCache(BaseCache* _cache){
            cache = _cache;
            assert(cache != NULL);
        }
        
        /**
        * This method sets the number of cpus and the cpu id of the associated
        * cache for the directory protocol. It is called in the cache 
        * constructor.
        *
        * @param num_cpus The number of CPUs in the system.
        * @param cpuId    The CPU ID of the cache associated with this 
        *                 protocol.
        */
        void setCpuCount(int num_cpus, int cpuId){
            directoryCpuCount = num_cpus;
            directoryCpuID = cpuId;
        }
        
        /**
        * Registers the statistics variables with the M5 statistics module.
        */
        void regStats();
        
        /**
        * Writes a trace line into the trace file.
        *
        * @param cachename    The name of the cache that called this method
        * @param message      The message to write to the file
        * @param owner        The ID of the cache that owns the block
        * @param state        The state of the cache block
        * @param paddr        The address of the cache block
        * @param blkSize      The block size of the cache
        * @param presentFlags A bool array showing which caches have a copy of
        *                     the cache block
        */
        void writeTraceLine(const std::string cachename,
                            const std::string message,
                            const int owner,
                            const DirectoryState state,
                            const Addr paddr,
                            const Addr blkSize,
                            bool* presentFlags);
        
        /**
        * When this method is called, the coherence message profile is written
        * to the profile file and the counters are reset.
        */
        void dumpStats();
        
        /**
        * This method returns checks if a given address is owned.
        *
        * @param address The cache block address
        *
        * @return True if the cache block is owned
        */
        bool isOwned(Addr address);
        
        /**
        * Retrieves the CPU ID of the cache currently owning a block.
        *
        * @param address The cache block address.
        *
        * @return The CPU ID of the cache block owner.
        */
        int getOwner(Addr address);
        
        /**
        * This method sets the owner of a cache block.
        *
        * @param address  The cache block address.
        * @param newOwner The CPU ID of the new owner.
        */
        void setOwner(Addr address, int newOwner);
        
        /**
        * This method removes the owner of a given cache block.
        *
        * @param address The cache block address.
        */
        void removeOwner(Addr address);
        
        /**
        * This method is a part of the directory coherence interface and is 
        * documented in the subclasses.
        */
        virtual void sendDirectoryMessage(MemReqPtr& req, int lat) = 0;
    
        /**
        * This method is a part of the directory coherence interface and is 
        * documented in the subclasses.
        */
        virtual void sendNACK(MemReqPtr& req,
                              int lat,
                              int toID,
                              int fromID) = 0;
        
        /**
        * This method is a part of the directory coherence interface and is 
        * documented in the subclasses.
        */
        virtual bool doDirectoryAccess(MemReqPtr& req) = 0;
    
        /**
        * This method is a part of the directory coherence interface and is 
        * documented in the subclasses.
        */
        virtual bool doL1DirectoryAccess(MemReqPtr& req, BlkType* blk) = 0;
    
        /**
        * This method is a part of the directory coherence interface and is 
        * documented in the subclasses.
        */
        virtual bool handleDirectoryResponse(MemReqPtr& req,
                                             TagStore *tags) = 0;
    
        /**
        * This method is a part of the directory coherence interface and is 
        * documented in the subclasses.
        */
        virtual bool handleDirectoryFill(MemReqPtr& req,
                                         BlkType* blk,
                                         MemReqList& writebacks,
                                         TagStore* tags) = 0;
    
        /**
        * This method is a part of the directory coherence interface and is 
        * documented in the subclasses.
        */
        virtual bool doDirectoryWriteback(MemReqPtr& req) = 0;
    
        /**
        * This method is a part of the directory coherence interface and is 
        * documented in the subclasses.
        */
        virtual MemAccessResult handleL1DirectoryMiss(MemReqPtr& req) = 0;
        

};

/**
* This class implements an event that dumps message statistics to a file.
*
* @author Magnus Jahre
*/
template <class TagStore>
class DirectoryProtocolDumpEvent : public Event
{

    public:
        
        DirectoryProtocol<TagStore>* protocol;
        int dumpInterval;
        
        /**
        * This constructor initialises the member variables with the provided
        * arguments.
        *
        * @param _protocol     A pointer to the directory protocol used
        * @param _dumpInterval The number of clock cycles between each dump
        */
        DirectoryProtocolDumpEvent(DirectoryProtocol<TagStore>* _protocol,
                                   int _dumpInterval)
            : Event(&mainEventQueue)
        {
            protocol = _protocol;
            dumpInterval = _dumpInterval;
        }

        /**
        * This method is called when the event is serviced. It makes the 
        * protocol dump the gathered statistics and reschedules itself.
        */
        void process(){
            protocol->dumpStats();
            this->schedule(curTick + dumpInterval);
        }

        /**
        * @return A textual description of the event.
        */
        virtual const char *description(){
            return "DirectoryProtocol dump event";
        }
};

#endif //__DIRECTORY_PROTOCOL_HH__
