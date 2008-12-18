
#ifndef __RING_HH__
#define __RING_HH__

#include "address_dependent_ic.hh"

class Ring : public AddressDependentIC{ 
    
    
    public:
        Ring(const std::string &_name, 
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

#endif //__RING_HH__