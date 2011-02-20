
#include "datadump.hh"
#include <fstream>

using namespace std;

DataDump::DataDump(const char* _filename){
	filename = _filename;
}

void
DataDump::addElement(string key, RequestTraceEntry value){
	assert(data.find(key) == data.end());
	data.insert(pair<string, RequestTraceEntry>(key, value));
}

void
DataDump::dump(){
    ofstream dumpfile(filename);

    map<string, RequestTraceEntry>::iterator it = data.begin();
    for( ; it != data.end(); it++){
        switch(it->second.type){
            case TICK_TRACE:
            	dumpfile << it->first << "=" << it->second.tickVal << "\n";
                break;
            case ADDR_TRACE:
            	dumpfile << it->first << "=" << it->second.addrVal << "\n";
                break;
            case INT_TRACE:
            	dumpfile << it->first << "=" << it->second.intVal << "\n";
                break;
            case DOUBLE_TRACE:
            	dumpfile << it->first << "=" << it->second.doubleVal << "\n";
                break;
            case STR_TRACE:
            	dumpfile << it->first << "=" << it->second.strVal << "\n";
                break;
            default:
            	fatal("Unknown trace type");
        }
    }

    dumpfile.flush();
    dumpfile.close();
}

string
DataDump::buildKey(string name, int id){
	stringstream strstream;
	strstream << name << "=" << id;
	return strstream.str();
}
