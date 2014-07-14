#include "itca.hh"
#include "sim/builder.hh"

using namespace std;

char*
ITCA::cpuStallSignalNames[ITCA_CPU_STALL_CNT] = {
    (char*) "dispatch",
    (char*) "register rename",
    (char*) "commit"
};

char*
ITCA::mainSignalNames[ITCA_SIGNAL_CNT] = {
    (char*) "ITCA_IT_INSTRUCTION",
    (char*) "ITCA_ROB_EMPTY",
    (char*) "ITCA_INTER_TOP_ROB",
    (char*) "ITCA_CPU_STALLED",
    (char*) "ITCA_ALL_MSHRS_INTER"
};

ITCA::ITCA(std::string _name, int _cpuID, ITCACPUStalls _cpuStall, ITCAInterTaskInstructionPolicy _itip, bool _doVerification)
: SimObject(_name){

	accountingState = ITCAAccountingState();
	accountingState.setCPUID(_cpuID);

	signalState = ITCASignalState();

	cpuID = _cpuID;
	useCPUStallSignal = _cpuStall;
	useITIP = _itip;

	lastSampleAt = 0;
	headOfROBAddr = 0;

	initVerificationTrace(_doVerification);
}

void
ITCA::initVerificationTrace(bool doTrace){
	debugtrace = RequestTrace(name(), "VerificationTrace", !doTrace);

	vector<string> headers;
	for(int i=0;i<ITCA_SIGNAL_CNT;i++){
		headers.push_back(mainSignalNames[i]);
	}
	headers.push_back("DO_NOT_ACCOUNT");
	debugtrace.initalizeTrace(headers);
}

void
ITCA::traceSignals(bool doNotAccount){
	vector<RequestTraceEntry> entries;
	bool allFalse = true;
	bool onlyCPUStalledTrue = true;
	bool onlyROBEmpty = true;
	for(int i=0;i<ITCA_SIGNAL_CNT;i++){
		entries.push_back(RequestTraceEntry(signalState.signalOn[i] ? 1 : 0));
		if(signalState.signalOn[i]) allFalse = false;
		if(signalState.signalOn[i] && i != ITCA_CPU_STALLED) onlyCPUStalledTrue = false;
		if(signalState.signalOn[i] && i != ITCA_ROB_EMPTY) onlyROBEmpty = false;
	}
	entries.push_back(RequestTraceEntry(doNotAccount ? 1 : 0));

	if(!(allFalse || onlyCPUStalledTrue || onlyROBEmpty)){
		debugtrace.addTrace(entries);
	}
}

void
ITCA::updateInterTopROB(){
	bool found = false;
	for(int i=0;i<dataMissTable.size();i++){
		if(dataMissTable[i].intertaskMiss && dataMissTable[i].addr == headOfROBAddr){
			assert(!found);
			found = true;
		}
	}
	if(found) signalState.set(ITCA_INTER_TOP_ROB);
	else signalState.clear(ITCA_INTER_TOP_ROB);

	DPRINTF(ITCA, "Signal ITCA_INTER_TOP_ROB set to %s, head of ROB addr %d\n",
			signalState.signalOn[ITCA_INTER_TOP_ROB] ? "ON" : "OFF",
			headOfROBAddr);
}

void
ITCA::checkAllMSHRsInterSig(){
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

	signalState.signalOn[ITCA_ALL_MSHRS_INTER] = newState;
	DPRINTF(ITCA, "Signal ALL_MSHRS_INTER set to %s\n",newState ? "ON" : "OFF");
}

void
ITCA::updateInterTaskInstruction(){
	bool interTaskCnt = 0;
	for(int i=0;i<instructionMissTable.size();i++){
		if(instructionMissTable[i].intertaskMiss){
			interTaskCnt++;
		}
	}

	if(useITIP == ITCA_ITIP_ONE && interTaskCnt >= 1){
		signalState.set(ITCA_IT_INSTRUCTION);
		DPRINTF(ITCA, "Setting ITCA_IT_INSTRUCTION with %d intertask instruction misses (%d total)\n",
				interTaskCnt,
				instructionMissTable.size());
	}
	else if(useITIP == ITCA_ITIP_ALL && interTaskCnt == instructionMissTable.size()){
		signalState.set(ITCA_IT_INSTRUCTION);
		DPRINTF(ITCA, "Setting ITCA_IT_INSTRUCTION with %d intertask instruction misses (%d total)\n",
						interTaskCnt,
						instructionMissTable.size());
	}
	else{
		DPRINTF(ITCA, "ITCA_IT_INSTRUCTION not set (%d instruction misses)\n", instructionMissTable.size());
		signalState.clear(ITCA_IT_INSTRUCTION);
	}
}

void
ITCA::processSignalChange(){

	// Update ITCA internal signals
	updateInterTopROB();
	checkAllMSHRsInterSig();
	updateInterTaskInstruction();

	// This code implements Figure 5 from Luque et al. (2012)
	bool portOne = signalState.signalOn[ITCA_IT_INSTRUCTION] && signalState.signalOn[ITCA_ROB_EMPTY];
	bool portTwo = signalState.signalOn[ITCA_INTER_TOP_ROB] && signalState.signalOn[ITCA_CPU_STALLED];
	// When one or more of these signals are set, we do not account (last gate in Fig 5 should be an OR and not a NOR)
	bool doNotAccount = portOne || portTwo || signalState.signalOn[ITCA_ALL_MSHRS_INTER];

	DPRINTF(ITCA, "Processing signal change: Port 1 %s, Port 2 %s, Port 3 %s; Decision %s\n",
			portOne ? "on": "off",
			portTwo ? "on": "off",
			signalState.signalOn[ITCA_ALL_MSHRS_INTER] ? "on": "off",
			doNotAccount ? "on": "off");

	traceSignals(doNotAccount);
	accountingState.update(doNotAccount);

	DPRINTF(ITCA, "Status: %d cycles accounted, %d cycles not accounted\n",
			accountingState.accountedCycles,
			accountingState.notAccountedCycles);
}

void
ITCA::l1DataMiss(Addr addr){
	dataMissTable.push_back(ITCATableEntry(addr));
	DPRINTF(ITCA, "Adding address %d to the data table, %d pending misses\n",
			addr,
			dataMissTable.size());

	processSignalChange();
}

void
ITCA::l1InstructionMiss(Addr addr){
	instructionMissTable.push_back(ITCATableEntry(addr));
	DPRINTF(ITCA, "Adding address %d to the instruction table, %d pending misses\n",
			addr,
			instructionMissTable.size());

	processSignalChange();
}

void
ITCA::squash(Addr addr){
	DPRINTF(ITCA, "Address %d was squashed, attempting to remove it from both data and instruction tables\n", addr);
	removeTableEntry(&dataMissTable, addr, true);
	removeTableEntry(&instructionMissTable, addr, true);
}

void
ITCA::l1MissResolved(Addr addr, Tick willFinishAt, bool isDataMiss){
	ITCAMemoryRequestCompletionEvent* event = new ITCAMemoryRequestCompletionEvent(this, addr, isDataMiss);
	event->schedule(willFinishAt);

	DPRINTF(ITCA, "Miss for addr %d resolved, %s miss, scheduling handling for cycle %d\n",
			addr,
			isDataMiss ? "data" : "instruction",
			willFinishAt);
}

void
ITCA::handleL1MissResolvedEvent(Addr addr, bool isDataMiss){
	if(isDataMiss) removeTableEntry(&dataMissTable, addr);
	else removeTableEntry(&instructionMissTable, addr);
	processSignalChange();
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
		signalState.clear(ITCA_CPU_STALLED);
		processSignalChange();
	}
	else{
		DPRINTF(ITCA, "Ignoring CPU resume on signal %s\n", cpuStallSignalNames[type]);
	}
}

void
ITCA::setROBHeadAddr(Addr addr){
	headOfROBAddr = addr;
	DPRINTF(ITCA, "Stalled on load for address %d\n", addr);

	processSignalChange();
}

void
ITCA::clearROBHeadAddr(){
	headOfROBAddr = 0;
	DPRINTF(ITCA, "Oldest ROB load completed, head of rob addr is now %d\n", headOfROBAddr);

	processSignalChange();
}

int
ITCA::findTableEntry(std::vector<ITCATableEntry>* table, Addr addr, bool acceptNotFound){
	int foundAt = -1;
	for(int i=0;i<table->size();i++){
		if(table->at(i).addr == addr){
			assert(foundAt == -1);
			foundAt = i;
		}
	}
	if(!acceptNotFound) assert(foundAt != -1);
	return foundAt;
}

void
ITCA::removeTableEntry(std::vector<ITCATableEntry>* table, Addr addr, bool acceptNotFound){
	int foundAt = findTableEntry(table, addr, acceptNotFound);
	if(foundAt == -1){
		assert(acceptNotFound);
		return;
	}
	table->erase(table->begin()+foundAt);

	DPRINTF(ITCA, "Removed element at position %d, addr %d, new size is %d\n",
		 	foundAt,
		 	addr,
		 	table->size());
}

void
ITCA::setROBEmpty(){
	assert(!signalState.signalOn[ITCA_ROB_EMPTY]);
	signalState.set(ITCA_ROB_EMPTY);
	DPRINTF(ITCA, "Setting signal ITCA_ROB_EMPTY\n");
	processSignalChange();
}

void
ITCA::clearROBEmpty(){
	assert(signalState.signalOn[ITCA_ROB_EMPTY]);
	signalState.clear(ITCA_ROB_EMPTY);
	DPRINTF(ITCA, "Clearing signal ITCA_ROB_EMPTY\n");
	processSignalChange();
}

/// ***************************************************************************
/// ITCAAccountingState
/// ***************************************************************************

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

/// ***************************************************************************
/// ITCASignalState
/// ***************************************************************************

void
ITCA::ITCASignalState::set(ITCASignals signal){
	signalOn[signal] = true;
}

void
ITCA::ITCASignalState::clear(ITCASignals signal){
	signalOn[signal] = false;
}

#ifndef DOXYGEN_SHOULD_SKIP_THIS

BEGIN_DECLARE_SIM_OBJECT_PARAMS(ITCA)
    Param<int> cpu_id;
	Param<string> cpu_stall_policy;
	Param<string> itip;
	Param<bool> do_verification;
END_DECLARE_SIM_OBJECT_PARAMS(ITCA)

BEGIN_INIT_SIM_OBJECT_PARAMS(ITCA)
	INIT_PARAM_DFLT(cpu_id, "CPU ID", -1),
	INIT_PARAM_DFLT(cpu_stall_policy, "The signal that determines if the CPU is stalled", "rename"),
	INIT_PARAM_DFLT(itip, "How to handle intertask instruction misses", "one"),
	INIT_PARAM_DFLT(do_verification, "Turn on the verification trace (Warning: creates large files)", false)
END_INIT_SIM_OBJECT_PARAMS(ITCA)

CREATE_SIM_OBJECT(ITCA)
{
	ITCA::ITCACPUStalls cpuStall = ITCA::ITCA_REG_RENAME_STALL;
	if((string) cpu_stall_policy == "commit") cpuStall = ITCA::ITCA_COMMIT_STALL;
    else if((string) cpu_stall_policy == "dispatch") cpuStall = ITCA::ITCA_DISPATCH_STALL;
    else if((string) cpu_stall_policy == "rename") cpuStall = ITCA::ITCA_REG_RENAME_STALL;
    else fatal("Unknown cpu_stall_policy provided");

	ITCA::ITCAInterTaskInstructionPolicy itipval = ITCA::ITCA_ITIP_ONE;
	if((string) itip == "one") itipval = ITCA::ITCA_ITIP_ONE;
	else if((string) itip == "all") itipval = ITCA::ITCA_ITIP_ALL;
	else fatal("Unknown ITIP");

	return new ITCA(getInstanceName(),
    		         cpu_id,
    		         cpuStall,
    		         itipval,
    		         do_verification);
}

REGISTER_SIM_OBJECT("ITCA", ITCA)

#endif
