
#include "requesttrace.hh"
#include "sim/sim_exit.hh"
#include "sim/root.hh"
#include "base/misc.hh"

#include <fstream>
#include <sstream>

#define REQUEST_DUMP_INTERVAL 1000000

using namespace std;

RequestTrace::RequestTrace(std::string _simobjectname, const char* _filename){
    
    stringstream filenamestream;
    filenamestream << _simobjectname << _filename << ".txt";
    filename = filenamestream.str();
    
    curTracePos = 0;
    tracebuffer.resize(REQUEST_DUMP_INTERVAL, string(""));
    
    initialized = false;
}

void 
RequestTrace::initalizeTrace(std::vector<std::string>& headers){
    
    registerExitCallback(new RequestTraceCallback(this));
    initialized = true;
    
    ofstream tracefile(filename.c_str());
    tracefile << "Tick";
    for(int i=0;i<headers.size();i++){
        tracefile << ";" << headers[i];
    }
    tracefile << "\n";
}

void 
RequestTrace::addTrace(std::vector<RequestTraceEntry>& values){
    
    assert(isInitialized());
    
    stringstream tracestring;
    tracestring << curTick;
    for(int i=0;i<values.size();i++){
        switch(values[i].type){
            case TICK_TRACE:
                tracestring << ";" << values[i].tickVal;
                break;
            case ADDR_TRACE:
                tracestring << ";" << values[i].addrVal;
                break;
            case INT_TRACE:
                tracestring << ";" << values[i].intVal;
                break;
            case DOUBLE_TRACE:
                tracestring << ";" << values[i].doubleVal;
                break;
            case STR_TRACE:
                tracestring << ";" << values[i].strVal;
                break;
            default:
            fatal("Unknown trace type");
        }
    }
    
    tracebuffer[curTracePos] = tracestring.str();
    curTracePos++;
      
    if(curTracePos == tracebuffer.size()){
        dumpTracebuffer();
    }
    assert(curTracePos < tracebuffer.size());
}


void 
RequestTrace::dumpTracebuffer(){
    
    assert(isInitialized());
    
    if(!tracebuffer.empty()){

        ofstream tracefile(filename.c_str(), ofstream::app);
        
        for(int i=0;i<curTracePos;i++) tracefile << tracebuffer[i].c_str() << "\n";
        curTracePos = 0;

        tracefile.flush();
        tracefile.close();
    }
}

