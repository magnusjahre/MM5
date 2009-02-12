
#ifndef __REQUEST_TRACE_HH__
#define __REQUEST_TRACE_HH__

#include "base/callback.hh"
#include "sim/host.hh"
#include "arch/alpha/isa_traits.hh"

#include <vector>
#include <string>

typedef enum{
  TICK_TRACE,
  ADDR_TRACE,
  INT_TRACE,
  DOUBLE_TRACE,
  STR_TRACE
} TRACE_ENTRY_TYPE;

class RequestTraceEntry{

    private:
        void resetValues(){
            tickVal = 0;
            addrVal = 0;
            intVal = 0;
            doubleVal = 0.0;
        }

    public:
        Tick tickVal;
        Addr addrVal;
        int intVal;
        double doubleVal;
        char* strVal;
        TRACE_ENTRY_TYPE type;
        
        RequestTraceEntry(Tick _val){
            resetValues();
            tickVal = _val;
            type = TICK_TRACE;
        }
        
        RequestTraceEntry(Addr _val){
            resetValues();
            addrVal = _val;
            type = ADDR_TRACE;
        }
        
        RequestTraceEntry(int _val){
            resetValues();
            intVal = _val;
            type = INT_TRACE;
        }
        
        RequestTraceEntry(double _val){
            resetValues();
            doubleVal = _val;
            type = DOUBLE_TRACE;
        }
        
        RequestTraceEntry(const char* _val){
            resetValues();
            strVal = (char*) _val;
            type = STR_TRACE;
        }
};

class RequestTrace{
    
    private:
        int curTracePos;
        std::vector<std::string> tracebuffer;
        std::string filename;
        bool initialized;
    
    public:
        
        RequestTrace(){ 
            initialized = false;
        }
        
        RequestTrace(std::string _simobjectname, const char* _filename);
    
        void initalizeTrace(std::vector<std::string>& headers);
        
        void addTrace(std::vector<RequestTraceEntry>& values);
        
        void dumpTracebuffer();
        
        bool isInitialized(){
            return initialized;
        }
};

class RequestTraceCallback : public Callback
{
    private:
        RequestTrace *rt;
    public:
        RequestTraceCallback(RequestTrace *r) : rt(r) {}
        virtual void process() { rt->dumpTracebuffer(); };
};


#endif //__REQUEST_TRACE_HH__

