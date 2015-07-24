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

	if(_doVerification){
		ITCATestEvent* event = new ITCATestEvent(this);
		event->schedule(curTick+1);
	}
}

void
ITCA::updateInterTopROB(){
	bool found = false;
	for(int i=0;i<dataMissTable.size();i++){
		if(dataMissTable[i].intertaskMiss) assert(dataMissTable[i].cpuAddr != 0);
		if(dataMissTable[i].intertaskMiss && dataMissTable[i].cpuAddr == headOfROBAddr){
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
ITCA::runITCALogic(){
	bool portOne = signalState.signalOn[ITCA_IT_INSTRUCTION] && signalState.signalOn[ITCA_ROB_EMPTY];
	DPRINTF(ITCA, "IT_INSTRUCTION %s, ROB_EMPTY %s; Decision %s\n",
			      signalState.signalOn[ITCA_IT_INSTRUCTION] ? "on": "off",
			      signalState.signalOn[ITCA_ROB_EMPTY] ? "on": "off",
				  portOne ? "on": "off");

	bool portTwo = signalState.signalOn[ITCA_INTER_TOP_ROB] && signalState.signalOn[ITCA_CPU_STALLED];
	DPRINTF(ITCA, "INTER_TOP_ROB %s, CPU_STALLED %s; Decision %s\n",
				  signalState.signalOn[ITCA_INTER_TOP_ROB] ? "on": "off",
				  signalState.signalOn[ITCA_CPU_STALLED] ? "on": "off",
				  portTwo ? "on": "off");

	// When one or more of these signals are set, we do not account (last gate in Fig 5 should be an OR and not a NOR)
	bool doNotAccount = portOne || portTwo || signalState.signalOn[ITCA_ALL_MSHRS_INTER];

	DPRINTF(ITCA, "Port 1 %s, Port 2 %s, Port 3 %s; Decision %s\n",
			portOne ? "on": "off",
			portTwo ? "on": "off",
			signalState.signalOn[ITCA_ALL_MSHRS_INTER] ? "on": "off",
			doNotAccount ? "on": "off");

	accountingState.update(doNotAccount);
	accountingState.updatePerfModStall(doNotAccount, portTwo);
}

void
ITCA::processSignalChange(){

	// Update ITCA internal signals
	updateInterTopROB();
	checkAllMSHRsInterSig();
	updateInterTaskInstruction();

	// This method implements Figure 5 from Luque et al. (2012)
	runITCALogic();

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
ITCA::intertaskMiss(Addr addr, bool isInstructionMiss, Addr cpuAddr){
	vector<ITCATableEntry>* table = &dataMissTable;
	if(isInstructionMiss) table = &instructionMissTable;

	int entryID = findTableEntry(table, addr);
	table->at(entryID).intertaskMiss = true;
	table->at(entryID).cpuAddr = cpuAddr;

	DPRINTF(ITCA, "Address %d is an %s intertask miss (CPU address %d)\n",
			addr,
			isInstructionMiss ? "instruction" : "data",
			cpuAddr);

	processSignalChange();
}

ITCA::ITCAAccountingInfo
ITCA::getAccountedCycles(){
	Tick sampleSize = curTick - lastSampleAt;

	ITCAAccountingInfo info;
	accountingState.handleSampleTransition(sampleSize);
	accountingState.handlePerfModSampleTransition(sampleSize);
	info.accountedCycles = accountingState.accountedCycles;
	info.perfModStallCycles = accountingState.perfModStallCycles;

	DPRINTF(ITCAProgress, "SAMPLING, accounted %d cycles, %d stall cycles, cycles in sample %d\n",
			info.accountedCycles,
			info.perfModStallCycles,
			sampleSize);

	accountingState.reset();
	lastSampleAt = curTick;

	return info;
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

	perfModStall = false;
	perfModStateChangedAt = 0;
	perfModStallCycles = 0;
	perfModNotStalledCycles = 0;
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
ITCA::ITCAAccountingState::updatePerfModStall(bool stopAccounting, bool isPerfModStall){
	Tick length = curTick - perfModStateChangedAt;

	if(perfModStall && stopAccounting){
		perfModStall = false;
		perfModStallCycles += length;
		perfModStateChangedAt = curTick;

		DPRINTFR(ITCA, "CPU %d STALL: Accounted %d cycles, last change at %d\n",
				cpuID,
				length,
				perfModStateChangedAt);

		assert(perfModNotStalledCycles + perfModStallCycles == curTick);
	}
	else if(!perfModStall && !stopAccounting && isPerfModStall){
		perfModStall = true;
		perfModNotStalledCycles += length;
		perfModStateChangedAt = curTick;

		DPRINTFR(ITCA, "CPU %d STALL: Accounting was off for %d cycles, last change at %d\n",
				cpuID,
				length,
				perfModNotStalledCycles);

		assert(perfModNotStalledCycles + perfModStallCycles == curTick);
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
ITCA::ITCAAccountingState::handlePerfModSampleTransition(Tick sampleSize){
	Tick length = curTick - perfModStateChangedAt;
	if(perfModStall){
		perfModStallCycles += length;
		DPRINTFR(ITCA, "CPU %d STALL: Accounting the final %d cycles of the sample\n",
				cpuID,
				length);
	}
	else{
		perfModNotStalledCycles += length;
		DPRINTFR(ITCA, "CPU %d STALL: Not accounting the final %d cycles of the sample\n",
				cpuID,
				length);
	}
	perfModStateChangedAt = curTick;

	assert(perfModNotStalledCycles + perfModStallCycles == sampleSize);
}

void
ITCA::ITCAAccountingState::reset(){
	// Note: stateChangedAt and perfModStateChanged at are reset in the handleSampleTransition methods
	// Since no signals have changed, there is no accounting state change
	accountedCycles = 0;
	notAccountedCycles = 0;

	perfModNotStalledCycles = 0;
	perfModStallCycles = 0;
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

/// ***************************************************************************
/// Unit Tests
/// ***************************************************************************

void
ITCA::testSignals(){
	cout << "\nVerifying ITCA state processing\n\n";

	bool expectedAccounting[32] = {true, true, true, false,
			                       true, true, true, false,
			                       true, true, true, false,
		  			               false, false, false, false,
		  			               false, false, false, false,
		  			               false, false, false, false,
		  			               false, false, false, false,
		  			               false, false, false, false};
	double errors = 0.0;
	for(int i=0;i<32;i++){

		cout << "Testing state " << i << ":\n";
		int mask = 16;
		for(int j=4;j>=0;j--){
			int state = mask & i;
			mask = mask >> 1;
			if(state != 0) signalState.set((ITCASignals) j);
			else signalState.clear((ITCASignals) j);
			cout << "Setting " << mainSignalNames[j] << " to " <<  (signalState.signalOn[j] ? 1 : 0) << "\n";
		}

		runITCALogic();

		cout << "Expected " << (expectedAccounting[i] ? "account" : "don't account" )
		     << ", got " << (accountingState.accounting ? "accounting" : "not accounting")
		     << ": ";

		if(accountingState.accounting == expectedAccounting[i]){
			cout << "OK!\n\n";
		}
		else{
			errors++;
			cout << "Failed...\n\n";
		}
	}

	if(errors > 0.0) fatal("Verification failed, %d correct", (32.0-errors)/32.0);
	else fatal("Verification completed successfully");
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
