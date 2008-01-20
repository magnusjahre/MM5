
#ifndef __TimeMultiplexedBus_BUS_HH__
#define __TimeMultiplexedBus_BUS_HH__

#include "mem/bus/bus.hh"

class TimeMultiplexedBus : public Bus
{
    private:
        
        int curAddrNum;
        int curDataNum;
        
        Tick lastAddrArb;
        Tick lastDataArb;
        
        static const int NO_REQ_DELAY = 1;
    
    public:

        TimeMultiplexedBus(const std::string &name,
                           HierParams *hier_params,
                           int width,
                           int clockRate,
                           AdaptiveMHA* _adaptiveMHA,
                           int _busCPUCount,
                           int _busBankCount);
        
        virtual void arbitrateAddrBus();
        
//         virtual void arbitrateDataBus();
        
        virtual void setBlocked(int id){
            fatal("Time Multiplexed bus blocking not implemented");
        }
        
        virtual void clearBlocked(int id){
            fatal("Time Multiplexed bus blocking not implemented");
        }
        
//     protected:
//         virtual void scheduleArbitrationEvent(Event * arbiterEvent, Tick reqTime,
//                                               Tick nextFreeCycle, Tick idleAdvance = 1);
        
    private:
        int getFairNextInterface(int & counter, std::vector<BusRequestRecord> & requests);
};

#endif // __TimeMultiplexed_BUS_HH__
