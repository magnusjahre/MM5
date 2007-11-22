
#include <iostream>
#include <vector>
        
#include "interconnect_interface.hh"

using namespace std;

void
InterconnectInterface::setBlocked(){
    if(!blocked){
        blocked = true;
        thisInterconnect->setBlocked(interfaceID);
    }
}

void
InterconnectInterface::clearBlocked(){
    if(blocked){
        blocked = false;
        thisInterconnect->clearBlocked(interfaceID);
    }
}

void
InterconnectInterface::getRange(std::list<Range<Addr> > &range_list)
{
    for (int i = 0; i < ranges.size(); ++i) {
        range_list.push_back(ranges[i]);
    }
}

void
InterconnectInterface::rangeChange(){
    thisInterconnect->rangeChange();
}


void
InterconnectInterface::setAddrRange(list<Range<Addr> > &range_list){
    ranges.clear();
    while (!range_list.empty()) {
        ranges.push_back(range_list.front());
        range_list.pop_front();
    }
    rangeChange();
}

void
InterconnectInterface::addAddrRange(const Range<Addr> &range){
    ranges.push_back(range);
    rangeChange();
}

void
InterconnectInterface::getSendSample(int* data,
                                     int* inst,
                                     int* coherence,
                                     int* total){
    
    // start profiling if this is the first call to this function
    if(!doProfiling) doProfiling = true;
    
    // return the sampled values
    *data = dataSends;
    *inst = instSends;
    *coherence = coherenceSends;
    *total = totalSends;
    
    // reset counters
    dataSends = 0;
    instSends = 0;
    coherenceSends = 0;
    totalSends = 0;
}

void
InterconnectInterface::updateProfileValues(MemReqPtr &req){
    
    if(doProfiling){
        if(req->cmd.isDirectoryMessage()){
            coherenceSends++;
        }
        else if(req->readOnlyCache){
            instSends++;
        }
        else{
            dataSends++;
        }
        totalSends++;
    }
}
