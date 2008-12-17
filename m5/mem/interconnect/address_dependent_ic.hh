
#ifndef __ADDRESS_DEPENDENT_IC_HH__
#define __ADDRESS_DEPENDENT_IC_HH__

#include "interconnect.hh"

class AddressDependentIC : public Interconnect
{
    protected:
        std::vector<bool> blockedLocalQueues;
        std::vector<int> notRetrievedRequests;
    
        void initQueues(int localBlockedSize, int expectedInterfaces);
        
    public:
        AddressDependentIC(const std::string &_name, 
                           int _width, 
                           int _clock,
                           int _transDelay,
                           int _arbDelay,
                           int _cpu_count,
                           HierParams *_hier,
                           AdaptiveMHA* _adaptiveMHA);
        
        virtual void send(MemReqPtr& req, Tick time, int fromID) = 0;
                
        virtual void arbitrate(Tick time) = 0;
        
        virtual void deliver(MemReqPtr& req, Tick cycle, int toID, int fromID) = 0;
        
        void request(Tick time, int fromID);
        
        void retriveRequest(int fromInterface);
        
        void setBlockedLocal(int fromCPUId);
        
        void clearBlockedLocal(int fromCPUId);
        
        int getChannelCount(){
            //one channel for all cpus, all banks and one coherence bus
            fatal("no");
            return 0;
        }
        
        std::vector<int> getChannelSample(){
            fatal("ni");
        }
        
        void writeChannelDecriptor(std::ofstream &stream){
            fatal("ni");
        }
        
        std::vector<std::vector<int> > retrieveInterferenceStats(){
            std::vector<std::vector<int> > retval(cpu_count, vector<int>(cpu_count, 0));
            return retval;
        }

        void resetInterferenceStats(){
        }
};

class ADIRetrieveReqEvent : public Event
{
    
    public:
        
        AddressDependentIC* adi;
        int fromID;
        
        ADIRetrieveReqEvent(AddressDependentIC* _adi, int _fid)
            : Event(&mainEventQueue)
        {
            adi = _adi;
            fromID = _fid;
        }

        void process(){
            adi->retriveRequest(fromID);
            delete this;
        }

        const char *description(){
            return "AddressDependentIC retrive event\n";
        }
};

class ADIArbitrationEvent : public Event
{
    
    public:
        
        AddressDependentIC* adi;
        
        ADIArbitrationEvent(AddressDependentIC* _adi)
            : Event(&mainEventQueue, Memory_Controller_Pri)
        {
            adi = _adi;
        }

        void process(){
            adi->arbitrate(curTick);
        }
        

        const char *description(){
            return "AddressDependentIC deliver event\n";
        }
};

class ADIDeliverEvent : public Event
{
    
    public:
        
        AddressDependentIC* adi;
        MemReqPtr req;
        bool toSlave;
        
        ADIDeliverEvent(AddressDependentIC* _adi, MemReqPtr& _req, bool _toSlave)
            : Event(&mainEventQueue)
        {
            adi = _adi;
            req = _req;
            toSlave = _toSlave;
        }

        void process(){
            if(toSlave) adi->deliver(req, curTick, req->toInterfaceID, req->fromInterfaceID);
            else adi->deliver(req, curTick, req->fromInterfaceID, req->toInterfaceID);
            delete this;
        }

        const char *description(){
            return "AddressDependentIC arbitration event\n";
        }
};

#endif //__ADDRESS_DEPENDENT_IC_HH__
