
#ifndef __BUTTERFLY__HH__
#define __BUTTERFLY__HH__

#include "interconnect.hh"

        
/**
* This class implements a butterfly interconnect. It was only developed to 
* investigate the performance of a multistage interconnection network.
* Consequently, is only possible to configure it to represent a subset of all
* possible butterfly networks.
*
* In particular, it handles 2, 4 or 8 processor cores. The reason is that the 
* mapping from interface to butterfly node is defined in the constructor. Since
* 8 processors are the maximum number used in this work, it was not prioritised
* to add support for more processors. Furthermore, only radix 2 switches is 
* supported and only L2 caches with 4 banks.
*
* Path diversity can be added to a butterfly by adding extra stages. This
* implementation has no path diversity.
*
* @author Magnus Jahre
*/
class Butterfly : public Interconnect
{
    private:
        int switchDelay;
        int radix;
        int butterflyCpuCount;
        int butterflyCacheBanks;
        int terminalNodes;
        int stages;
        int switches;
        int butterflyHeight;
        int hopCount;
        int chanBetweenStages;
        
        std::map<int, int> cpuIDtoNode;
        std::map<int, int> l2IDtoNode;
        
//         std::list<InterconnectRequest*> requestQueue;
//         std::list<InterconnectDelivery*> grantQueue;
//         std::vector<int> blockedInterfaces;
        
        std::vector<bool> butterflyStatus;
        std::vector<int> channelUsage;
        
    
    public:
        
        /**
        * This constructor creates the interface to node mapping for a given
        * number of CPUs. Furthermore, a number of convenience values are
        * computed. Examples of such values are the width and height of the
        * butterfly.
        *
        * @param _name        The name given in the configuration file
        * @param _width       The width of the transmission channels
        * @param _clock       The number of processor clock cycles in one 
        *                     interconnect clock cycle
        * @param _transDelay  The transfer delay _per_ _channel_ in the
        *                     butterfly
        * @param _arbDelay    Arbitration delay for the Interconnect 
        *                     constructor. This should be set to 0 as there is
        *                     no explicit arbitration in a butterfly.
        * @param _cpu_count   The number of cpus in the system
        * @param _hier        Hierarchy parameters for BaseHier
        * @param _switchDelay The delay through the switches in the butterfly
        * @param _radix       The number of inputs or outputs for each switch 
        *                     (only 2 are supported in this implementation).
        * @param _banks       The number of L2 banks (only 4 are supported in 
        *                     this implementation).
        */
        Butterfly(const std::string &_name, 
                  int _width, 
                  int _clock,
                  int _transDelay,
                  int _arbDelay,
                  int _cpu_count,
                  HierParams *_hier,
                  int _switchDelay,
                  int _radix,
                  int _banks,
                  AdaptiveMHA* _adaptiveMHA);
        
        /**
        * This destructor does nothing.
        */
        ~Butterfly(){
            /* noop */
        }
        
        /**
        * This method is called from an interface when it is granted access. It
        * computes the interface ID of the recipient based on the request given
        * and adds this to a delivery queue. Then, it schedules a delivery 
        * event if needed.
        *
        * @param req    The memory request to send.
        * @param time   The clock cycle the method was called at.
        * @param fromID The interface ID of the sender interface.
        */
        void send(MemReqPtr& req, Tick time, int fromID);
        
        /**
        * This method is called when an arbitration event is serviced. It
        * attempts to grant access to as many interfaces as possible given the
        * limitations of the butterfly interconnect. Since the request queue
        * is sorted, the older requests are prioritised.
        *
        * If all requests can not be granted at a given cycle, an arbitration
        * event is scheduled at the next clock cycle if at least one request is
        * old enough to be scheduled at this cycle. If not, an arbitration 
        * event is added at the request time + arbitration delay.
        *
        * @param cycle The clock cycle the method is called.
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
        * This method returns the number of transmission channels in the
        * interconnect and is used by the InterconnectProfile class.
        *
        * @return The number of transmission channels
        *
        * @see InterconnectProfile
        */
        int getChannelCount();
        
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
    
    private:
        
        bool setChannelsOccupied(int fromInterfaceID, int toInterfaceID);
        
        void printChannelStatus();
};

#endif //__BUTTERFLY__HH__

