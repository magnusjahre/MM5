
#include <iostream>
#include <vector>
        
#include "crossbar_interface.hh"

using namespace std;

void
CrossbarInterface::setBlocked(){
    if(!blocked){
        blocked = true;
        thisCrossbar->setBlocked(interfaceID);
    }
}

void
CrossbarInterface::clearBlocked(){
    if(blocked){
        blocked = false;
        thisCrossbar->clearBlocked(interfaceID);
    }
}

void
CrossbarInterface::getRange(std::list<Range<Addr> > &range_list)
{
    for (int i = 0; i < ranges.size(); ++i) {
        range_list.push_back(ranges[i]);
    }
}

void
CrossbarInterface::rangeChange(){
    thisCrossbar->rangeChange();
}


void
CrossbarInterface::setAddrRange(list<Range<Addr> > &range_list){
    ranges.clear();
    while (!range_list.empty()) {
        ranges.push_back(range_list.front());
        range_list.pop_front();
    }
    rangeChange();
}

void
CrossbarInterface::addAddrRange(const Range<Addr> &range){
    ranges.push_back(range);
    rangeChange();
}
