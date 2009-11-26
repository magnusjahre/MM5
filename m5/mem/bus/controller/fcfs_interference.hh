
#ifndef FCFS_CONTROLLER_INTERFERENCE_HH_
#define FCFS_CONTROLLER_INTERFERENCE_HH_

#include "controller_interference.hh"

class FCFSControllerInterference : public ControllerInterference{

private:
	bool privateStorageInited;
	std::vector<std::vector<MemReqPtr> > privateRequestQueues;

public:

	FCFSControllerInterference(const std::string& _name,
							   int _cpu_cnt,
							   TimingMemoryController* _ctrl);

	void insertRequest(MemReqPtr& req);

	void estimatePrivateLatency(MemReqPtr& req);

	void initialize(int cpu_count){
		fatal("initalize() is not needed in FCFS interference implementation");
	}

	bool isInitialized(){
		fatal("isInitialized() is not needed in FCFS interference implementation");
	}

	virtual void insertPrivateVirtualRequest(MemReqPtr& req){
		//NOTE: private virtual requests are not supported in FCFS interference
	}
};

#endif
