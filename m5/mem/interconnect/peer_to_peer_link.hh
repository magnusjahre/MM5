
#ifndef __PEER_TO_PEER_LINK_HH__
#define __PEER_TO_PEER_LINK_HH__

#include "address_dependent_ic.hh"

class PeerToPeerLink : public AddressDependentIC{ 
    
    private:

        std::list<MemReqPtr> p2pRequestQueue;
        std::list<MemReqPtr> p2pResponseQueue;
        
        ADIArbitrationEvent* arbEvent;
        
        int queueSize;
        int slaveInterconnectID;
        int attachedCPUID;
        
        Tick nextLegalArbTime;
        
        std::list<MemReqPtr> deliveryBuffer;
        
        void retrieveAdditionalRequests();
        
        inline bool isWaitingRequests(){
            return (!p2pRequestQueue.empty() && !blockedInterfaces[0]) || !p2pResponseQueue.empty();
        }
    
    public:
        PeerToPeerLink(const std::string &_name, 
                       int _width, 
                       int _clock,
                       int _transDelay,
                       int _arbDelay,
                       int _cpu_count,
                       HierParams *_hier,
                       AdaptiveMHA* _adaptiveMHA);
        
        virtual void send(MemReqPtr& req, Tick time, int fromID);
        
        virtual void arbitrate(Tick time);
        
        virtual void deliver(MemReqPtr& req, Tick cycle, int toID, int fromID);
        
        virtual void clearBlocked(int fromInterface);
};

#endif //__PEER_TO_PEER_LINK_HH__