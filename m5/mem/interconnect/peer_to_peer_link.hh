
#ifndef __PEER_TO_PEER_LINK_HH__
#define __PEER_TO_PEER_LINK_HH__

#include "address_dependent_ic.hh"

class PeerToPeerLink : public AddressDependentIC{ 
    
    private:
            
        struct P2PRequestEntry{
            MemReqPtr req;
            Tick entry;
            
            P2PRequestEntry(MemReqPtr& _req, Tick _entry){
                req = _req;
                entry = _entry;
            }
        };
            
        std::vector<std::list<P2PRequestEntry> > p2pRequestQueues;
        std::list<MemReqPtr> p2pResponseQueue;
        
        ADIArbitrationEvent* arbEvent;
        
        int queueSize;
    
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
};

#endif //__PEER_TO_PEER_LINK_HH__