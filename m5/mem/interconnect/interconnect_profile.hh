
#ifndef __INTERCONNECT_PROFILE_HH__
#define __INTERCONNECT_PROFILE_HH__

#include "sim/sim_object.hh"
#include "sim/eventq.hh"
#include "interconnect.hh"

/** The number of clock cycles between each profile event */
#define RESOLUTION 250000
        
class Interconnect;
class InterconnectProfileEvent;
        
/** Differentiates send profile events from channel profile events */
typedef enum {SEND, CHANNEL} INTERCONNECT_PROFILE_TYPE;
        
/**
* This class implements a simple profiler for interconnects. It retrieves some
* statistics from the interconnect object and writes it to a file at a regular
* interval. This interval is decided by the RESOLUTION definition at the 
* begining of this file.
*
* Currently, this class provides two forms of profiles:
* - Firstly, it profiles the number of sends injected into the interconnect.
*   This gives a measure of the external pressure on the interconnect as a
*   function of time.
* - Secondly, it prints the utilisation of each channel inside the 
*   interconnect at regular interval. This feature can be used to identify
*   bottlenecks inside a given interconnect.
*
* @author Magnus Jahre
*/
class InterconnectProfile : public SimObject
{
    private:
        Interconnect* interconnect;
        bool traceSends;
        bool traceChannelUtil;
        Tick startTick;
        
        InterconnectProfileEvent* sendEvent;
        InterconnectProfileEvent* channelEvent;
        
        std::string sendFileName;
        std::string channelFileName;
        std::string channelExplFileName;
    
    public:
        /**
        * This constructor creates the interconnect profiler and schedules the
        * first profile event.
        *
        * @param _name             The name from the config file
        * @param _traceSends       Wheter or not sends should be traced or not
        * @param _traceChannelUtil Wheter or not sends should be traced or not
        * @param _startTick        The clock cycle the profiling will start
        * @param _interconnect     A pointer to the interconnect that will be 
        *                          profiled
        */
        InterconnectProfile(const std::string &_name,
                            bool _traceSends,
                            bool _traceChannelUtil,
                            Tick _startTick,
                            Interconnect* _interconnect);
        
        /**
        * This method initialises the send profile file.
        */
        void initSendFile();
        
        /**
        * This method is called when a profile event i serviced. It retrieves
        * the current profile values from the interconnect and writes them to
        * the send profile file.
        */
        void writeSendEntry();
        
        /**
        * This method initialises the channel file. It checks if the 
        * interconnect supports channel profiling and initalises the file if it
        * does. The reason for this check is that channel profiling makes no
        * sense with an ideal interconnect.
        *
        * @return True, if the interconnect supports channel profiling.
        */
        bool initChannelFile();
        
        /**
        * This method retrieves the updated channel utilisation from the 
        * interconnect and writes these values to the channel utilisation file.
        */
        void writeChannelEntry();
};

/**
* This class implements a profile event. It schedules itself at regular
* intervals when it has been started and calls the appropriate methods for the
* statistics to be written to the profile files.
*
* @author Magnus Jahre
*/
class InterconnectProfileEvent : public Event
{

    public:
        
        InterconnectProfile* profiler;
        INTERCONNECT_PROFILE_TYPE traceType;
        
        /**
        * Initalises the member variables.
        *
        * @param _profiler A pointer to the associated profiler object
        * @param _type     The type of profiler
        */
        InterconnectProfileEvent(InterconnectProfile* _profiler,
                                 INTERCONNECT_PROFILE_TYPE _type)
            : Event(&mainEventQueue)
        {
            profiler = _profiler;
            traceType = _type;
        }

        /**
        * This method is called when the event is serviced. It calls a method
        * of the profiler object according to the event type and schedules
        * itself RESOLUTION ticks later.
        */
        void process(){
            
            switch(traceType){
                case SEND:
                    profiler->writeSendEntry();
                    break;
                case CHANNEL:
                    profiler->writeChannelEntry();
                    break;
                default:
                    fatal("Unimplemented interconnect trace type");
            }
            
            this->schedule(curTick + RESOLUTION);
        }

        /**
        * @return A textual description of the event
        */
        virtual const char *description(){
            return "InterconnectProfileEvent";
        }
};

#endif //__INTERCONNECT_PROFILE_HH__
