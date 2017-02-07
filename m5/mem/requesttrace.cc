
#include "requesttrace.hh"
#include "sim/sim_exit.hh"
#include "sim/root.hh"
#include "base/misc.hh"

#include <fstream>
#include <sstream>

using namespace std;


RequestTraceEntry::RequestTraceEntry(Tick _val, TRACE_ENTRY_TYPE _type){
    resetValues();
    tickVal = _val;
    type = _type;
}

RequestTraceEntry::RequestTraceEntry(Addr _val){
    resetValues();
    addrVal = _val;
    type = ADDR_TRACE;
}

RequestTraceEntry::RequestTraceEntry(int _val){
    resetValues();
    intVal = _val;
    type = INT_TRACE;
}

RequestTraceEntry::RequestTraceEntry(double _val){
    resetValues();
    doubleVal = _val;
    type = DOUBLE_TRACE;
}

RequestTraceEntry::RequestTraceEntry(const char* _val){
    resetValues();
    strVal = (char*) _val;
    type = STR_TRACE;
}

RequestTrace::RequestTrace(std::string _simobjectname, const char* _filename, bool _disableTrace){

    stringstream filenamestream;
    filenamestream << _simobjectname << _filename << ".txt";
    filename = filenamestream.str();

    curTracePos = 0;
    initialized = false;
    traceDisabled = _disableTrace;

    if(!traceDisabled){
    	if(fileExists(".rundir")){
    		dumpInterval = 1000000;
    		warn("File .rundir present in current directory, setting dump interval to %d for file %s", dumpInterval, filename);
    	}
    	else{
    		dumpInterval = 1;
    	}

    	assert(dumpInterval > 0);

    	tracebuffer.resize(dumpInterval, string(""));
    }
}

bool
RequestTrace::fileExists(string name) {
    ifstream f(name.c_str());
    if (f.good()) {
        f.close();
        return true;
    } else {
        f.close();
        return false;
    }
}

void
RequestTrace::initalizeTrace(std::vector<std::string>& headers){

	if(traceDisabled) return;

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

	if(traceDisabled) return;

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
                tracestring << ";";
                tracestring.precision(10);
                tracestring << values[i].doubleVal;
                break;
            case STR_TRACE:
                tracestring << ";" << values[i].strVal;
                break;
            default:
            	fatal("Unknown trace type");
            	break;
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

	if(traceDisabled) return;

    assert(isInitialized());

    if(!tracebuffer.empty()){

        ofstream tracefile(filename.c_str(), ofstream::app);
        for(int i=0;i<curTracePos;i++) tracefile << tracebuffer[i].c_str() << "\n";
        curTracePos = 0;

        tracefile.flush();
        tracefile.close();
    }
}

std::string
RequestTrace::buildTraceName(const char* name, int id){
	stringstream strstream;
	strstream << name << " " << id;
	return strstream.str();
}

std::string
RequestTrace::buildFilename(const char* name, int id){
	stringstream strstream;
	strstream << name << id;
	return strstream.str();
}

