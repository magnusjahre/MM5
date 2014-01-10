
#include "requesttrace.hh"
#include "sim/sim_exit.hh"
#include "sim/root.hh"
#include "base/misc.hh"

#include <fstream>
#include <sstream>

using namespace std;

RequestTrace::RequestTrace(std::string _simobjectname, const char* _filename){

    stringstream filenamestream;
    filenamestream << _simobjectname << _filename << ".txt";
    filename = filenamestream.str();

    if(fileExists(".rundir")){
    	dumpInterval = 1000000;
    	warn("File .rundir present in current directory, setting dump interval to %d for file %s", dumpInterval, filename);
    }
    else{
    	dumpInterval = 1;
    }

    assert(dumpInterval > 0);
    curTracePos = 0;
    tracebuffer.resize(dumpInterval, string(""));

    initialized = false;


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
                tracestring << ";";
                tracestring.precision(10);
                tracestring << values[i].doubleVal;
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

