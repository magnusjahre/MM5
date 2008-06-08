
#ifndef __SPLIT_TRANS_BUS_HH__
#define __SPLIT_TRANS_BUS_HH__

#include <iostream>
#include <vector>
#include <queue>

#include "interconnect.hh"

#define DEBUG_SPLIT_TRANS_BUS

/**
* This class implements a Split Transaction Bus interconnect. Here, all 
* interfaces are connected to one transmission channel. After arbitration,
* a request is granted both the address bus and the data bus.
*
* Two bus types have been implemented. One version has pipelined arbitration
* and pipelined transfer while the other one is not pipelined. The pipelined
* version is not realistic as it assumes that a request can be injected into
* the data bus from any interface each clock cycle.
*
* @author Magnus Jahre
*/
class SplitTransBus : public Interconnect
{
    private:

        std::list<InterconnectRequest* > requestQueue;
        std::list<InterconnectDelivery* > deliverQueue;
        
        /* in a pipelined, bi-directional bus we can issue one request 
           in each direction each clock cycle */
        std::list<InterconnectRequest* >* slaveRequestQueue;
        
        bool pipelined;
        
        void addToList(std::list<InterconnectRequest*>* inList,
                       InterconnectRequest* icReq);
        
        typedef enum{STB_MASTER, STB_SLAVE, STB_NOT_PIPELINED} grant_type;
        void grantInterface(grant_type gt, Tick cycle);
        
        void scheduleArbitrationEvent(Tick possibleArbCycle);
        void scheduleDeliverEvent(Tick possibleArbCycle);
        
        bool doProfile;
        int useCycleSample;
        
#ifdef DEBUG_SPLIT_TRANS_BUS
        void checkIfSorted(std::list<InterconnectRequest*>* inList);
        void printRequestQueue();
        void printDeliverQueue();
#endif //DEBUG_SPLIT_TRANS_BUS
        
    public:
        
        /**
        * This constructor creates a split transaction bus object. If the bus 
        * is not pipelined the arbitration delay must be longer or equal to the
        * transfer delay. The reason is that the arbitration method assumes 
        * that the previous bus transfer has finished when an arbitration 
        * operation finishes.
        *
        * @param _name       The name provided in the config file
        * @param _width      The bus width in bytes
        * @param _clock      The number of processor clock in one bus cycle
        * @param _transDelay The number of bus cycles one transfer takes
        * @param _arbDelay   The number of bus cycles one arbitration takes
        * @param _cpu_count  The number of processors in the system
        * @param _hier       Hierarchy params for BaseHier
        */
        SplitTransBus(const std::string &_name,
                      int _width, 
                      int _clock,
                      int _transDelay,
                      int _arbDelay,
                      int _cpu_count,
                      bool _pipelined,
                      HierParams *_hier,
                      AdaptiveMHA* _amha)
            : Interconnect(_name,
                           _width, 
                           _clock, 
                           _transDelay, 
                           _arbDelay,
                           _cpu_count,
                           _hier,
                           _amha){
            
            pipelined = _pipelined;
            
            if(arbitrationDelay < transferDelay && !pipelined){
                fatal("This bus implementation requires the arbitration "
                      "delay to be longer than or equal to the transfer "
                      "delay");
            }
            
            doProfile = false;
            useCycleSample = 0;
            
            if(pipelined){
                slaveRequestQueue = new std::list<InterconnectRequest*>;
            }
        }
        
        /**
        * Default destructor. Deletes the dynamically allocated
        * slaveRequestQueue if the bus is pipelined.
        */
        ~SplitTransBus(){
            if(pipelined){
                delete slaveRequestQueue;
            }
        }
        
        /**
        * This method is called when a interface needs to use the bus. It adds 
        * the request to a queue and schedules an arbitration event. If the bus
        * is pipelined, there are two request queues. The reason is that there
        * are two buses in this case. One runs from the slave interfaces to the
        * master interfaces and one in the opposite direction.
        *
        * @param time   The clock cycle the request is requested
        * @param fromID The ID of the interface requesting access
        */
        void request(Tick time, int fromID);

        /**
        * This methods takes creates an InterconnectDelivery object based on
        * the arguments given. Then, an InterconnectDeliverQueueEvent is
        * scheduled after the specified transmission delay. The request 
        * queue(s) are kept sorted in ascending order with the oldest
        * request first.
        *
        * @param req    The memory request
        * @param time   The clock cycle the method is called in
        * @param fromID The ID of the interface sending the request
        *
        * @see InterconnectDelivery
        * @see InterconnectDeliverQueueEvent
        */
        void send(MemReqPtr& req, Tick time, int fromID);
        
        /**
        * This method is called when an arbitration event is serviced. Each
        * time it is called, it issues at least one request. In the 
        * non-pipelined version the oldest request is granted access.
        * In the pipelined version, the oldest master request and the oldest
        * slave request are granted access.
        *
        * The method assumes that the request queues are sorted.
        *
        * @param cycle The clock cycle the arbitration method is called
        */
        void arbitrate(Tick cycle);
        
        /**
        * This method is called when a InterconnectDeliverQueueEvent is 
        * serviced. It delivers one request each time it is called. If the
        * cache does not block and there are more requests that need to be 
        * delivered, it checks whether a delivery event has been registered.
        * This is needed because there might be requests waiting from an 
        * earlier cache blocking.
        *
        * @param req    The request to deliver
        * @param cycle  The clock tick the method was called
        * @param toID   The interface ID of the destination interface
        * @param fromID The interface ID of the sender interface
        */
        void deliver(MemReqPtr& req, Tick cycle, int toID, int fromID);
        
        /**
        * This method is called if one of the slave cache banks blocks. Then,
        * it removes all arbitration and deliver events. Requests that arrive
        * while a cache bank is blocked are simply queued.
        *
        * @param fromInterface The interface that is blocked
        */
        void setBlocked(int fromInterface);
        
        /**
        * This method is called when a slave cache can recieve requests again.
        *
        * @param fromInterface The interface that is no longer blocked
        */
        void clearBlocked(int fromInterface);
        
        
        /**
        * This method is called from the InterconnectProfile class when it
        * needs to know how many transmission channels the bus has.
        *
        * @return 1 since the bus only has one channel
        *
        * @see InterconnectProfile
        */
        int getChannelCount(){
            return 1;
        }
        
        /**
        * This method is called from the InterconnectProfile class and returns
        * the number of clock cycles the bus was in use since the last time it
        * was called.
        *
        * @return The number of clock cycles the bus was used since the last
        *         time the method was called.
        *
        * @see InterconnectProfile
        */
        std::vector<int> getChannelSample();
        
        /**
        * This method writes a desciption of the transmission channels used to
        * the provided stream.
        *
        * @param stream The stream to write to
        */
        void writeChannelDecriptor(std::ofstream &stream){
            stream << "0: The shared bus\n";
        }

};
#endif // SPLIT_TRANS_BUS_HH
