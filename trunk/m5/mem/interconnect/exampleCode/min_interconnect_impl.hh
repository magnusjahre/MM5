
#ifndef __IDEAL_INTERCONNECT_HH__
#define __IDEAL_INTERCONNECT_HH__

#include <iostream>
#include <vector>
#include <queue>

#include "interconnect.hh"


class IdealInterconnect : public Interconnect
{
    
    private:
        
        
    public:
        
        IdealInterconnect(const std::string &_name,
                      int _width, 
                      int _clock,
                      int _transDelay,
                      int _arbDelay,
                      HierParams *_hier)
            : Interconnect(_name,
                           _width, 
                           _clock, 
                           _transDelay, 
                           _arbDelay,
                           _hier){
        
        if(_width != 0) fatal("The ideal interconnect has no width");
        
        std::cout << "ideal interconnect created: clock " << _clock << " transDelay " << _transDelay << " arb delay " << _arbDelay <<  "\n";
        
        }
        
        ~IdealInterconnect(){ /* does nothing */ }
        
        void request(Tick time, int fromID);

        void send(MemReqPtr& req, Tick time, int fromID);
        
        void arbitrate(Tick cycle);
        
        void deliver(MemReqPtr& req, Tick cycle, int toID, int fromID);
        
        void setBlocked(int fromInterface);
        
        void clearBlocked(int fromInterface);
    
};

#endif // __IDEAL_INTERCONNECT_HH__
