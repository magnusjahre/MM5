
#ifndef __ADAPTIVE_MHA_HH__
#define __ADAPTIVE_MHA_HH__

#include "sim/sim_object.hh"
#include "mem/cache/base_cache.hh"
#include "cpu/base.hh"
#include "encumbered/cpu/full/cpu.hh"
#include "mem/bus/bus.hh"
#include "mem/mem_req.hh"
#include "sim/eventq.hh"

class BaseCache;
class AdaptiveMHASampleEvent;

enum InterferenceType{
    INTERCONNECT_INTERFERENCE,
    L2_INTERFERENCE,
    MEMORY_INTERFERENCE,
    INTERFERENCE_COUNT
};

class AdaptiveMHA : public SimObject{
    
    private:
        
        int adaptiveMHAcpuCount;
        Tick sampleFrequency;
        double highThreshold;
        double lowThreshold;
        int neededRepeatDecisions;
        
        bool useFairAMHA;
        
        int maxMshrs;
        
        std::vector<BaseCache* > dataCaches;
        std::vector<BaseCache* > instructionCaches;
        
        std::vector<int> staticAsymmetricMHAs;
        
        int currentCandidate;
        int numRepeatDecisions;
        
        Bus* bus;
        
        AdaptiveMHASampleEvent* sampleEvent;
        
        std::string memTraceFileName;
        std::string adaptiveMHATraceFileName;
        std::string ipcTraceFileName;
        
        bool firstSample;
        
        bool onlyTraceBus;
        
        std::vector<FullCPU* > cpus;
        
        std::vector<std::vector<Tick> > totalInterferenceDelay;
        std::vector<Tick> totalSharedDelay;
        std::vector<Tick> totalSharedWritebackDelay;
        int interferenceOverflow;
        
        int numInterferenceRequests;
        int numDelayRequests;
        
        struct delayEntry{
            std::vector<std::vector<Tick> > cbDelay;
            std::vector<std::vector<Tick> > l2Delay;
            std::vector<std::vector<Tick> > memDelay;
            Tick totalDelay;
            bool isRead;
            
            delayEntry(){
                totalDelay = 0;
                isRead = true;
            }
            
            delayEntry(const delayEntry &entry){
                cbDelay = entry.cbDelay;
                l2Delay = entry.l2Delay;
                memDelay = entry.memDelay;
                totalDelay = entry.totalDelay;
                isRead = entry.isRead;
            }
            
            delayEntry(std::vector<std::vector<Tick> > _delays, bool read, InterferenceType type);
            
            void addDelays(std::vector<std::vector<Tick> > newDelays, InterferenceType type);
            
            bool committed(){
                return (totalDelay != 0);
            }
        };
        
        std::map<Addr,delayEntry> oracleStorage;
    
    public:
        
        Stats::Scalar<> dataSampleTooLarge;
        Stats::Scalar<> addrSampleTooLarge;
        
        AdaptiveMHA(const std::string &name,
                    double _lowThreshold,
                    double _highThreshold,
                    int cpu_count,
                    Tick _sampleFrequency,
                    Tick _startTick,
                    bool _onlyTraceBus,
                    int _neededRepeatDecisions,
                    std::vector<int> & _staticAsymmetricMHA,
                    bool _useFairAMHA);
        
        ~AdaptiveMHA();
        
        void regStats();
        
        void registerCache(int cpu_id, bool isDataCache, BaseCache* cache);
        
        void registerBus(Bus* _bus){
            bus = _bus;
            assert(bus != NULL);
        }
        
        void handleSampleEvent(Tick time);
        
        int getCPUCount(){
            return adaptiveMHAcpuCount;
        }
        
        int getSampleSize(){
            return sampleFrequency;
        }
        
        void registerFullCPU(int id, FullCPU* cpu){
            assert(id < cpus.size());
            assert(cpus[id] == 0);
            cpus[id] = cpu;
        }
        
        void addInterferenceDelay(std::vector<std::vector<Tick> > perCPUQueueTimes,
                                  Addr addr,
                                  MemCmd cmd,
                                  int fromCPU,
                                  InterferenceType type);
        void addTotalDelay(int issuedCPU, Tick delay, Addr addr, bool isRead);
        
    private:
        
        void doFairAMHA();
        
        void doThroughputAMHA(double dataBusUtil, std::vector<int> dataUsers);

        void throughputDecreaseNumMSHRs(std::vector<int> currentVector);
        
        void throughputIncreaseNumMSHRs();

};

class AdaptiveMHASampleEvent : public Event
{

    public:
        
        AdaptiveMHA* adaptiveMHA;
        
        AdaptiveMHASampleEvent(AdaptiveMHA* _adaptiveMHA)
            : Event(&mainEventQueue), adaptiveMHA(_adaptiveMHA)
        {
        }
        
        void process(){
          adaptiveMHA->handleSampleEvent(this->when());
        }

        virtual const char *description(){
            return "AdaptiveMHASampleEvent";
        }
};

#endif //__ADAPTIVE_MHA_HH__
