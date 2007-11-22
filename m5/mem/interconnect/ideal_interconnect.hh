
#ifndef __IDEAL_INTERCONNECT_HH__
#define __IDEAL_INTERCONNECT_HH__

#include <iostream>
#include <vector>
#include <list>

#include "interconnect.hh"

#define DEBUG_IDEAL_INTERCONNECT

/**
* This class implements an ideal interconnect. Ideal in this context means that
* a request will experience the specified delay, but an unlimited number of 
* requests will be granted access in parallel. The rationale for this choice
* it that transmission delays are mainly due to physical factors that can not
* be changed by architectural techniques.
*
* @author Magnus Jahre
*/
class IdealInterconnect : public Interconnect
{
    
    private:
        
#ifdef DEBUG_IDEAL_INTERCONNECT
        void printRequestQueue();
        void printGrantQueue();
#endif //DEBUG_IDEAL_INTERCONNECT
        
    public:
        
        /**
        * This constructor initialises a few member variables and passes on 
        * parameters to the Interconnect constructor. The cache implementation
        * requires that the interconnect has a finite width. Consequently, a
        * width equal to the cache block size should be provided.
        *
        * @param _name       The object name from the configuration file. This
        *                    is passed on to BaseHier and SimObject
        * @param _width      The bit width of the transmission lines in the
        *                    interconnect
        * @param _clock      The number of processor cycles in one interconnect
        *                    clock cycle.
        * @param _transDelay The end-to-end transfer delay through the 
        *                    interconnect in CPU cycles
        * @param _arbDelay   The lenght of an arbitration in CPU cycles
        * @param _cpu_count  The number of processors in the system
        * @param _hier       Hierarchy parameters for BaseHier
        *
        * @see Interconnect
        */
        IdealInterconnect(const std::string &_name,
                      int _width, 
                      int _clock,
                      int _transDelay,
                      int _arbDelay,
                      int _cpu_count,
                      HierParams *_hier)
            : Interconnect(_name,
                           _width, 
                           _clock, 
                           _transDelay, 
                           _arbDelay,
                           _cpu_count,
                           _hier){
        
            if(_width <= 0){
                fatal("The idealInterconnect must have a finite width, "
                      "or else the cache implementation won't work");
            }
            
            transferDelay = _transDelay;
            arbitrationDelay = _arbDelay;
        
        }
        
        /**
        * Empty destructor.
        */
        ~IdealInterconnect(){ /* does nothing */ }
        
        /**
        * The send method is called when an interface is granted access and
        * finds the destination interface from the values in the request. Then,
        * it adds the request to a delivery queue and schedules a delivery
        * event if needed.
        *
        * @param req    The memory request to send.
        * @param time   The clock cycle the method was called at.
        * @param fromID The interface ID of the sender interface.
        */
        void send(MemReqPtr& req, Tick time, int fromID);
        
        /**
        * The ideal interconnect arbitration method grants access to all 
        * requests that can be granted access every time it runs. A request
        * can be granted if it has experienced the specified arbitration 
        * delay.
        *
        * @param cycle The clock cycle the method was called.
        */
        void arbitrate(Tick cycle);
        
        /**
        * This method tries to deliver as many requests as possible to its
        * destination. Only, requests that have experienced the defined delay
        * can be delivered. However, if an L2 bank blocks, all requests that
        * are old enough might not be delivered. Since the delivery queue
        * is kept sorted, the oldest requests are delivered first.
        *
        * Since this class uses a delivery queue, all parameters except
        * cycle are discarded.
        *
        * @param req    Not used, must be NULL.
        * @param cycle  The clock cycle the method is called.
        * @param toID   Not used, must be -1.
        * @param fromID Not used, must be -1.
        */
        void deliver(MemReqPtr& req, Tick cycle, int toID, int fromID);
        
        /**
        * Since there are an unlimited number of channels in the ideal 
        * interconnect, this method returns -1.
        *
        * @return Always -1
        *
        * @see InterconnectProfile
        */
        int getChannelCount(){
            return -1;
        }
        
        /**
        * It makes no sense to get a channel sample in an ideal interconnect,
        * so if this method is called it prints a fatal error message.
        *
        * @return An empty vector
        */
        std::vector<int> getChannelSample(){
            fatal("Ideal Interconnect has no channels");
            std::vector<int> retval;
            return retval;
        }
        
        /**
        * The ideal interconnect has an unlimited number of channels.
        * Consequently, this method reports a fatal error if it is called.
        * 
        * @param stream The stream that is never used
        */
        void writeChannelDecriptor(std::ofstream &stream){
            fatal("Ideal Interconnect has no channel descriptor");
        }
};

#endif // __IDEAL_INTERCONNECT_HH__
