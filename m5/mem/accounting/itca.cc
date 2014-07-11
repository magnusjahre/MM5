#include "itca.hh"
#include "sim/builder.hh"

using namespace std;

char*
ITCA::cpuStallSignalNames[ITCA_CPU_STALL_CNT] = {
    (char*) "dispatch",
    (char*) "register rename",
    (char*) "ROB"
};

ITCA::ITCA(std::string _name, int _cpuID, ITCACPUStalls _cpuStall) : SimObject(_name){

	accountingState = ITCAAccountingState();
	accountingState.setCPUID(_cpuID);

	signalState = ITCASignalState();

	cpuID = _cpuID;
	useCPUStallSignal = _cpuStall;
	lastSampleAt = 0;
}

void
ITCA::processSignalChange(){

	// This code implements Figure 5 from Luque et al. (2012)
	bool portOne = signalState.signalOn[ITCA_IT_INSTRUCTION] && signalState.signalOn[ITCA_ROB_EMPTY];
	bool portTwo = signalState.signalOn[ITCA_INTER_TOP_ROB] && signalState.signalOn[ITCA_CPU_STALLED];
	// When one or more of these signals are set, we do not account (last gate in Fig 5 should be an OR and not a NOR)
	bool doNotAccount = portOne || portTwo || signalState.signalOn[ITCA_ALL_MSHRS_INTER];

	DPRINTF(ITCA, "SignalChange: Port 1 %s, Port 2 %s, Port 3 %s; Decision %s\n",
			portOne ? "on": "off",
			portTwo ? "on": "off",
			signalState.signalOn[ITCA_ALL_MSHRS_INTER] ? "on": "off",
			doNotAccount ? "on": "off");

	accountingState.update(doNotAccount);
}

void
ITCA::l1DataMiss(Addr addr){
	dataMissTable.push_back(ITCATableEntry(addr));
	DPRINTF(ITCA, "Adding address %d to the data table, %d pending misses\n",
			addr,
			dataMissTable.size());

	checkAllMSHRsInterSig();
}

void
ITCA::l1MissResolved(Addr addr, Tick willFinishAt){
	ITCAMemoryRequestCompletionEvent* event = new ITCAMemoryRequestCompletionEvent(this, addr);
	event->schedule(willFinishAt);

	DPRINTF(ITCA, "Miss for addr %d resolved, scheduling handling for cycle %d\n",
			addr,
			willFinishAt);
}

void
ITCA::handleL1MissResolvedEvent(Addr addr){
	removeTableEntry(&dataMissTable, addr);
	checkAllMSHRsInterSig();
}

void
ITCA::intertaskMiss(Addr addr, bool isInstructionMiss){
	vector<ITCATableEntry>* table = &dataMissTable;
	if(isInstructionMiss) table = &instructionMissTable;

	int entryID = findTableEntry(table, addr);
	table->at(entryID).intertaskMiss = true;

	DPRINTF(ITCA, "Address %d is an %s intertask miss\n",
			isInstructionMiss ? "instruction" : "data",
			addr);

	checkAllMSHRsInterSig();
}

void
ITCA::checkAllMSHRsInterSig(){
	bool prevState = signalState.signalOn[ITCA_ALL_MSHRS_INTER];
	bool newState = true;

	if(dataMissTable.empty()){
		newState = false;
	}
	else{
		for(int i=0;i<dataMissTable.size();i++){
			if(!dataMissTable[i].intertaskMiss){
				newState = false;
			}
		}
	}

	DPRINTF(ITCA, "Signal ALL_MSHRS_INTER set to %s\n",
			newState ? "ON" : "OFF");

	signalState.signalOn[ITCA_ALL_MSHRS_INTER] = newState;
	if(newState != prevState){
		processSignalChange();
	}
}

void
ITCA::setSignal(ITCASignals signal){
	signalState.set(signal);
	processSignalChange();
}

void
ITCA::unsetSignal(ITCASignals signal){
	signalState.unset(signal);
	processSignalChange();
}

Tick
ITCA::getAccountedCycles(){
	Tick sampleSize = curTick - lastSampleAt;

	accountingState.handleSampleTransition(sampleSize);
	Tick accountedCycles = accountingState.accountedCycles;

	DPRINTF(ITCAProgress, "SAMPLING, accounted %d cycles, cycles in sample %d\n",
			accountedCycles,
			sampleSize);

	accountingState.reset();
	lastSampleAt = curTick;

	return accountedCycles;
}

void
ITCA::itcaCPUStalled(ITCACPUStalls type){
	if(type == useCPUStallSignal){
		DPRINTF(ITCA, "CPU stall on signal %s detected, setting CPU stall signal\n", cpuStallSignalNames[type]);
		signalState.set(ITCA_CPU_STALLED);
		processSignalChange();
	}
	else{
		DPRINTF(ITCA, "Ignoring CPU stall on signal %s\n", cpuStallSignalNames[type]);
	}
}

void
ITCA::itcaCPUResumed(ITCACPUStalls type){
	if(type == useCPUStallSignal){
		DPRINTF(ITCA, "CPU resumed execution on signal %s, unsetting CPU stall signal\n", cpuStallSignalNames[type]);
		signalState.unset(ITCA_CPU_STALLED);
		processSignalChange();
	}
	else{
		DPRINTF(ITCA, "Ignoring CPU resume on signal %s\n", cpuStallSignalNames[type]);
	}
}

ITCA::ITCAAccountingState::ITCAAccountingState(){
	cpuID = -1;
	accounting = true;
	stateChangedAt = 0;
	accountedCycles = 0;
	notAccountedCycles = 0;
}

void
ITCA::ITCAAccountingState::update(bool doNotAccount){
	Tick length = curTick - stateChangedAt;
	if(accounting && doNotAccount){
		accounting = false;
		accountedCycles += length;

		DPRINTFR(ITCA, "CPU %d: Accounted %d cycles, last change at %d\n",
				 cpuID,
				 length,
				 stateChangedAt);

		stateChangedAt = curTick;
	}
	else if(!accounting && !doNotAccount){
		accounting = true;
		notAccountedCycles += length;

		DPRINTFR(ITCA, "CPU %d: Accounting was off for %d cycles, last change at %d\n",
				 cpuID,
				 length,
				 stateChangedAt);

		stateChangedAt = curTick;
	}
}

void
ITCA::ITCAAccountingState::handleSampleTransition(Tick sampleSize){
	Tick length = curTick - stateChangedAt;

	if(accounting){
		accountedCycles += length;

		DPRINTFR(ITCA, "CPU %d: Accounting the final %d cycles of the sample\n",
				 cpuID,
				 length);
	}
	else{
		notAccountedCycles += length;

		DPRINTFR(ITCA, "CPU %d: Not accounting the final %d cycles of the sample\n",
				 cpuID,
				 length);
	}

	stateChangedAt = curTick;

	assert(accountedCycles + notAccountedCycles == sampleSize);
}

void
ITCA::ITCAAccountingState::reset(){
	// Note: stateChangedAt is reset in handleSampleTransition()
	// Since no signals have changed, there is no accounting state change
	accountedCycles = 0;
	notAccountedCycles = 0;
}

void
ITCA::ITCASignalState::set(ITCASignals signal){
	assert(!signalOn[signal]);
	signalOn[signal] = true;
}

void
ITCA::ITCASignalState::unset(ITCASignals signal){
	assert(signalOn[signal]);
	signalOn[signal] = false;
}

int
ITCA::findTableEntry(std::vector<ITCATableEntry>* table, Addr addr){
	int foundAt = -1;
	for(int i=0;i<table->size();i++){
		if(table->at(i).addr == addr){
			assert(foundAt == -1);
			foundAt = i;
		}
	}
	assert(foundAt != -1);
	return foundAt;
}

void
ITCA::removeTableEntry(std::vector<ITCATableEntry>* table, Addr addr){
	int foundAt = findTableEntry(table, addr);
	table->erase(table->begin()+foundAt);

	DPRINTF(ITCA, "Removed element at position %d, addr %d, new size is %d\n",
		 	foundAt,
		 	addr,
		 	table->size());
}
#ifndef DOXYGEN_SHOULD_SKIP_THIS

BEGIN_DECLARE_SIM_OBJECT_PARAMS(ITCA)
    Param<int> cpu_id;
	Param<string> cpu_stall_policy;
END_DECLARE_SIM_OBJECT_PARAMS(ITCA)

BEGIN_INIT_SIM_OBJECT_PARAMS(ITCA)
	INIT_PARAM_DFLT(cpu_id, "CPU ID", -1),
	INIT_PARAM_DFLT(cpu_stall_policy, "The signal that determines if the CPU is stalled", "rename")
END_INIT_SIM_OBJECT_PARAMS(ITCA)

CREATE_SIM_OBJECT(ITCA)
{
	ITCA::ITCACPUStalls cpuStall = ITCA::ITCA_REG_RENAME_STALL;
	if((string) cpu_stall_policy == "rob") cpuStall = ITCA::ITCA_ROB_STALL;
    else if((string) cpu_stall_policy == "dispatch") cpuStall = ITCA::ITCA_DISPATCH_STALL;
    else if((string) cpu_stall_policy == "rename") cpuStall = ITCA::ITCA_REG_RENAME_STALL;
    else fatal("Unknown cpu_stall_policy provided");

	return new ITCA(getInstanceName(),
    		         cpu_id,
    		         cpuStall);
}

REGISTER_SIM_OBJECT("ITCA", ITCA)

#endif
