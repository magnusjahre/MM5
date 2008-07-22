
#ifndef __CROSSBAR_HH__
#define __CROSSBAR_HH__

#include <iostream>
#include <vector>
#include <queue>

#include "interconnect.hh"

#define DEBUG_CROSSBAR

/**
* This class implements a crossbar interconnect inspired by the crossbar used
* in IBM's Power 4 and Power 5 processors. Here, two crossbar connects all L1
* caches to all L2 banks. One crossbar is added in the L1 to L2 direction and 
* the other crossbar runs in the L2 to L1 direction. L1 to L1 transfers are
* made possible by connecting all L1 caches to a shared bus. Instruction and
* data caches for the same core share address and data lines.
*
* The crossbar modelled in this class differs from the IBM design in one way:
* - The IBM crossbar only has address lines in the L2 to L1 direction. This
*   implementation has address lines in both directions.
*
* Arbitration and transfer in the crossbar are pipelined.
*
* @author Magnus Jahre
*/
class Crossbar : public Interconnect
{
    
    private:
        
        bool isFirstRequest;
        Tick nextBusFreeTime;
        
        bool doProfiling;
        std::vector<int> channelUseCycles;
        
        bool doFairArbitration;
        std::vector<Tick> virtualFinishTimes;
        
        bool checkCrossbarState(InterconnectRequest* req,
                                int toInterfaceID,
                                std::vector<bool>* state,
                                bool* busIsUsed,
                                Tick cycle);
        
        Tick getMinStartTag();
        
        void grantInterface(InterconnectRequest* req,
                            int toInterfaceID,
                            Tick cycle,
                            std::vector<int> &grantedCPUs,
                            std::vector<int> &toBanks,
                            std::vector<Addr> &destinationAddrs,
                            std::vector<MemCmd> &currentCommands,
                            int position = 0);
        
        void doStandardArbitration(Tick candiateReqTime,
                                   std::list<InterconnectRequest* > &notGrantedReqs,
                                   Tick cycle,
                                   bool& busIsUsed,
                                   std::vector<bool>& occupiedEndNodes,
                                   std::vector<int> &grantedCPUs,
                                   std::vector<int> &toBanks,
                                   std::vector<Addr> &destinationAddrs,
                                   std::vector<MemCmd> &currentCommands);
        
        void doNFQArbitration(Tick candiateReqTime,
                              std::list<InterconnectRequest* > &notGrantedReqs,
                              Tick cycle,
                              bool& busIsUsed,
                              std::vector<bool>& occupiedEndNodes,
                              std::vector<int> &grantedCPUs,
                              std::vector<int> &toBanks,
                              std::vector<Addr> &destinationAddrs,
                              std::vector<MemCmd> &currentCommands);
        
        struct reqLess : public binary_function<InterconnectRequest*, InterconnectRequest* ,bool> {
            bool operator()(InterconnectRequest* a, InterconnectRequest* b){
                return a->time < b->time;
            }
        };
        
    public:
        
        /**
        * This constructor initialises a few member variables, but sends all 
        * parameters to the Interconnect constructor.
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
        Crossbar(const std::string &_name,
                 int _width, 
                 int _clock,
                 int _transDelay,
                 int _arbDelay,
                 int _cpu_count,
                 HierParams *_hier,
                 AdaptiveMHA* _adaptiveMHA,
                 bool _useNFQArbitration);
        
        /**
        * This destructor deletes the request queues that are dynamically
        * allocated when the first request is recieved.
        */
        ~Crossbar(){ }

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
        * The crossbar arbitration method removes the oldest request from each
        * request queue each cycle. The request must have experienced the
        * specified arbitration delay to be eligible for being granted access.
        * If all requests can not be granted, it attempts to schedule a new 
        * arbitration event.
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
        * This method returns the number of transmission channels and is used
        * by the InterconnectProfile class. In this crossbar implementation,
        * the number of channels is the number of interfaces plus the shared 
        * bus.
        *
        * @return The number of transmission channels
        *
        * @see InterconnectProfile
        */
        int getChannelCount(){
            //one channel for all cpus, all banks and one coherence bus
            channelUseCycles.resize(cpu_count + slaveInterfaces.size() + 1,0);
            return cpu_count + slaveInterfaces.size() + 1;
        }
        
        /**
        * This method returns the number of cycles the different channels was
        * occupied since it was called last.
        *
        * @return The number of clock cycles each channel was used since last
        *         time the method was called.
        *
        * @see InterconnectProfile
        */
        std::vector<int> getChannelSample();
        
        /**
        * This method writes a description of the different channels to
        * the provided stream.
        *
        * @param stream The output stream to write to.
        *
        * @see InterconnectProfile
        */
        void writeChannelDecriptor(std::ofstream &stream);
};

#endif // __CROSSBAR_HH__
