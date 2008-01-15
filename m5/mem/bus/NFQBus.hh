
#ifndef __NFQ_BUS_HH__
#define __NFQ_BUS_HH__

#include "mem/bus/bus.hh"

class NFQBus : public Bus
{
    private:
        
        Tick virtualAddrClock;
        Tick virtualDataClock;
        std::vector<Tick> lastAddrFinishTag;
        std::vector<Tick> lastDataFinishTag;
    
    public:

        NFQBus(const std::string &name,
            HierParams *hier_params,
            int width,
            int clockRate,
            AdaptiveMHA* _adaptiveMHA,
            int _busCPUCount,
            int _busBankCount);
        
        virtual void arbitrateAddrBus();
        
        virtual void arbitrateDataBus();
        
        virtual void setBlocked(int id){
            fatal("NFQ Bus blocking not implemented");
        }
        
        virtual void clearBlocked(int id){
            fatal("NFQ Bus blocking not implemented");
        }
        
    protected:
        virtual void scheduleArbitrationEvent(Event * arbiterEvent, Tick reqTime,
                                              Tick nextFreeCycle, Tick idleAdvance = 1);
        
    private:
        
        int getNFQNextInterface(std::vector<BusRequestRecord> & requests, std::vector<Tick> & finishTags, bool addr);
        
        void resetVirtualClock(bool found, std::vector<BusRequestRecord> & requests, Tick & clock, std::vector<Tick> & tags, Tick curStartTag, Tick oldest, bool addr);

};

#endif // __NFQ_BUS_HH__
