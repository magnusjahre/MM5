
#include "directory.hh"


/**
* This class implements the Stenstrom directory cache coherence protocol.
*
* @author Magnus Jahre
*/
template <class TagStore>
class StenstromProtocol : public DirectoryProtocol<TagStore>
{
    using DirectoryProtocol<TagStore>::cache;
    using DirectoryProtocol<TagStore>::directoryRequests;
    using DirectoryProtocol<TagStore>::cacheName;
    
    using DirectoryProtocol<TagStore>::numRedirectedReads;
    using DirectoryProtocol<TagStore>::numOwnerRequests;
    using DirectoryProtocol<TagStore>::numOwnerWritebacks;
    using DirectoryProtocol<TagStore>::numSharerWritebacks;
    using DirectoryProtocol<TagStore>::numNACKs;

    typedef typename TagStore::BlkType BlkType;
    
    private:
        DirectoryProtocol<TagStore>* parentPtr;
        
        std::map<Addr, bool*> outstandingWritebackWSAddrs;
        std::map<Addr, int> outstandingOwnerTransAddrs;
    
    public:
        
        /**
        * This constructor creates a stenstrom protocol object.
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
        StenstromProtocol(const std::string &_name,
                          const std::string &_protocol,
                          bool _doTrace,
                          int _dumpInterval,
                          int _traceStart):
            DirectoryProtocol<TagStore>(_name,
                                        _protocol,
                                        _doTrace,
                                        _dumpInterval,
                                        _traceStart)
        {
            parentPtr = dynamic_cast<DirectoryProtocol<TagStore>* >(this);
            assert(parentPtr != NULL);
        }
        
        /**
        * Depending on whether the cache is a L1 cache or an L2 cache, the 
        * details of sending a message is different. This method hides
        * these details.
        *
        * @param req The request to send.
        * @param lat The number of clock cycles before the request should be
        *            sent.
        */
        void sendDirectoryMessage(MemReqPtr& req, int lat);
    
        /**
        * Negative Acknowledge messages (NACKs) are issued quite often by the
        * protocol. This method hides the details of how they are sent.
        *
        * @param req    The request to send
        * @param lat    The latency before the request is sent
        * @param toID   The reciever CPU ID
        * @param fromID The sender CPU ID
        */
        void sendNACK(MemReqPtr& req, int lat, int toID, int fromID);
        
        /**
        * This message handles directory accesses in the L2 cache access 
        * method.
        *
        * @param req The current request
        *
        * @return True, if the request was handled by the protocol.
        */
        bool doDirectoryAccess(MemReqPtr& req);
    
        /**
        * This message handles directory accesses in the L1 cache access 
        * method. It is only called if it is a hit in the cache.
        *
        * @param req The current request
        * @param blk A pointer to the current cache block
        *
        * @return True, if the request was handled by the protocol.
        */
        bool doL1DirectoryAccess(MemReqPtr& req, BlkType* blk);
    
        /**
        * When a L1 cache recieves a response, some of these must be handled
        * without actually accessing the cache. These situations are handled
        * by this method.
        *
        * Some of these cases require accessing the tag store to aquire updated
        * information on a cache block. Consequently, the tag store is provided
        * as a parameter.
        *
        * @param req  The current request
        * @param tags A pointer to the cache tag store
        *
        * @return True, if the request was handled by the protocol.
        */
        bool handleDirectoryResponse(MemReqPtr& req, TagStore *tags);
    
        /**
        * When a cache fill is recieved, this must be checked by the coherence
        * protocol. These cases are handled by this method.
        *
        * @param req        The current request.
        * @param blk        The current cache block.
        * @param writebacks The current list of writebacks.
        * @param tags       A pointer to the cache's tag store.
        *
        * @return True, if the request was handled by the protocol.
        */
        bool handleDirectoryFill(MemReqPtr& req,
                                 BlkType* blk,
                                 MemReqList& writebacks,
                                 TagStore* tags);
    
        /**
        * Writebacks of shared blocks require special handling. This method 
        * checks the writebacks and handles them according to the protocol.
        *
        * @param req The writeback request
        *
        * @return True, if the request was handled by the protocol.
        */
        bool doDirectoryWriteback(MemReqPtr& req);
    
        /**
        * Some race conditions cause directory messages to miss in the L1 
        * cache. These cases are handled by this method.
        *
        * @param req The current memory request
        *
        * @return If the return value is different from BA_NO_RESULT, the 
        *         result should be used directly.
        */
        MemAccessResult handleL1DirectoryMiss(MemReqPtr& req);
        
    private:
        
        void setUpRedirectedRead(MemReqPtr& req,
                                 int fromProcessorID,
                                 int toProcessorID);
        
        void setUpRedirectedReadReply(MemReqPtr& req,
                                      int fromProcessorID,
                                      int toProcessorID);
        
        void setUpOwnerTransferInL2(MemReqPtr& req,
                                    int oldOwner,
                                    int newOwner);
        
        void setUpACK(MemReqPtr& req, int toID, int fromID);
};
