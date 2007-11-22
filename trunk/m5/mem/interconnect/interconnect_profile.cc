
#include "interconnect_profile.hh"
#include "sim/builder.hh"
        
#include <fstream>
        
using namespace std;
        
class Interconnect;
        
InterconnectProfile::InterconnectProfile(const std::string &_name,
                                         bool _traceSends,
                                         bool _traceChannelUtil,
                                         Tick _startTick,
                                         Interconnect* _interconnect) 
    : SimObject(_name)
{
    assert(_interconnect != NULL);
    
    traceSends = _traceSends;
    traceChannelUtil = _traceChannelUtil;
    startTick = _startTick;
    interconnect = _interconnect;
    
    interconnect->registerProfiler(this);
    
    sendFileName = "interconnectSendProfile.txt";
    channelFileName = "interconnectChannelProfile.txt";
    channelExplFileName = "interconnectChannelExplanation.txt";
    
    initSendFile();
    
    sendEvent = new InterconnectProfileEvent(this, SEND);
    sendEvent->schedule(startTick);
    
    bool doChannelTrace = initChannelFile();
    
    if(doChannelTrace){
        channelEvent = new InterconnectProfileEvent(this, CHANNEL);
        channelEvent->schedule(startTick);
    }
}

void
InterconnectProfile::initSendFile(){
    ofstream sendfile(sendFileName.c_str());
    sendfile << "Clock Cycle;Data Sends;Instruction Sends;"
             << "Coherence Sends;Total Sends\n";
    sendfile.flush();
    sendfile.close();
}

void
InterconnectProfile::writeSendEntry(){
    
    // get sample
    int data = 0, insts = 0, coherence = 0, total = 0;
    interconnect->getSendSample(&data, &insts, &coherence, &total);
    assert(data + insts + coherence == total);
    
    // write to file
    ofstream sendfile(sendFileName.c_str(), ofstream::app);
    sendfile << curTick << ";"
            << data << ";"
            << insts << ";"
            << coherence << ";"
            << total << "\n";
    sendfile.flush();
    sendfile.close();
}
        
bool
InterconnectProfile::initChannelFile(){
    
    int channelCount = interconnect->getChannelCount();
    
    if(channelCount != -1){
        
        // Write first line in tracefile
        ofstream chanfile(channelFileName.c_str());
        chanfile << "Clock Cycle; ";
        
        for(int i =0;i<channelCount;i++){
            chanfile << "Channel " << i;
            
            if(i == channelCount-1) chanfile << "\n";
            else chanfile << "; ";
        }
        
        chanfile.flush();
        chanfile.close();
        
        // Write channel explanation
        ofstream explFile(channelExplFileName.c_str());
        interconnect->writeChannelDecriptor(explFile);
        explFile.flush();
        explFile.close();
        
        return true;
    }
    
    //interconnect does not support channel profiling
    return false;
    
}
        
void
InterconnectProfile::writeChannelEntry(){
    
    int channelCount = interconnect->getChannelCount();
    vector<int> res = interconnect->getChannelSample();
    assert(res.size() == channelCount);
    
    ofstream channelfile(channelFileName.c_str(), ofstream::app);
    channelfile << curTick << ";";
    
    for(int i=0;i<res.size();i++){
        channelfile << ((double) res[i] / (double) RESOLUTION);
        if(i == res.size()-1) channelfile << "\n";
        else channelfile << ";";
    }

    channelfile.flush();
    channelfile.close();
}
        
#ifndef DOXYGEN_SHOULD_SKIP_THIS
        
BEGIN_DECLARE_SIM_OBJECT_PARAMS(InterconnectProfile)
    Param<bool> traceSends;
    Param<bool> traceChannelUtil;
    Param<Tick> traceStartTick;
    SimObjectParam<Interconnect*> interconnect;
END_DECLARE_SIM_OBJECT_PARAMS(InterconnectProfile)

BEGIN_INIT_SIM_OBJECT_PARAMS(InterconnectProfile)
    INIT_PARAM(traceSends, "Trace number of sends?"),
    INIT_PARAM(traceChannelUtil, "Trace channel utilisation?"),
    INIT_PARAM(traceStartTick, "The clock cycle to start the trace"),
    INIT_PARAM(interconnect, "The interconnect to profile")
END_INIT_SIM_OBJECT_PARAMS(InterconnectProfile)

CREATE_SIM_OBJECT(InterconnectProfile)
{
    return new InterconnectProfile(getInstanceName(),
                                   traceSends,
                                   traceChannelUtil,
                                   traceStartTick,
                                   interconnect);
}

REGISTER_SIM_OBJECT("InterconnectProfile", InterconnectProfile)

#endif //DOXYGEN_SHOULD_SKIP_THIS
