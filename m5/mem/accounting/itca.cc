

#include "itca.hh"

using namespace std;

ITCA::ITCA(int _cpuID){
	accountingState = ITCAAccountingState();
	accountingState.setCPUID(_cpuID);
	signalState = ITCASignalState();
	cpuID = _cpuID;
}

void
ITCA::processSignalChange(){

	// This code implements Figure 5 from Luque et al. (2012)
	bool portOne = signalState.signalOn[ITCA_IT_INSTRUCTION] && signalState.signalOn[ITCA_ROB_EMPTY];
	bool portTwo = signalState.signalOn[ITCA_INTER_TOP_ROB] && signalState.signalOn[ITCA_RENAME_STALLED];
	// When one or more of these signals are set, we do not account (last gate in Fig 5 should be an OR and not a NOR)
	bool doNotAccount = portOne || portTwo || signalState.signalOn[ITCA_ALL_MSHRS_INTER];

	DPRINTF(ITCA, "CPU %d: SignalChange: Port 1 %s, Port 2 %s, Port 3 %s; Decision %s\n",
			cpuID,
			portOne ? "on": "off",
			portTwo ? "on": "off",
			signalState.signalOn[ITCA_ALL_MSHRS_INTER] ? "on": "off",
			doNotAccount ? "on": "off");

	accountingState.update(doNotAccount);
}

void
ITCA::l1DataMiss(Addr addr){
	dataMissTable.push_back(ITCATableEntry(addr));
	DPRINTF(ITCA, "CPU %d: Adding address %d to the data table, %d pending misses\n",
			cpuID,
			addr,
			dataMissTable.size());

	checkAllMSHRsInterSig();
}

void
ITCA::l1MissResolved(Addr addr, Tick willFinishAt){
	ITCAMemoryRequestCompletionEvent* event = new ITCAMemoryRequestCompletionEvent(this, addr);
	event->schedule(willFinishAt);

	DPRINTF(ITCA, "CPU %d: Miss for addr %d resolved, scheduling handling for cycle %d\n",
			cpuID,
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

	DPRINTF(ITCA, "CPU %d: Address %d is an %s intertask miss\n",
			cpuID,
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

	DPRINTF(ITCA, "CPU %d: Signal ALL_MSHRS_INTER set to %s\n",
			cpuID,
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
ITCA::getAccountedCycles(Tick sampleSize){
	accountingState.handleSampleTransition(sampleSize);
	Tick accountedCycles = accountingState.accountedCycles;
	accountingState.reset();
	return accountedCycles;
}

ITCA::ITCAAccountingState::ITCAAccountingState(){
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

		DPRINTF(ITCA, "CPU %d: Accounted %d cycles, last change at %d\n",
				cpuID,
				length,
				stateChangedAt);

		stateChangedAt = curTick;
	}
	else if(!accounting && !doNotAccount){
		accounting = true;
		notAccountedCycles += length;

		DPRINTF(ITCA, "CPU %d: Accounting was off for %d cycles, last change at %d\n",
				cpuID,
				length,
				stateChangedAt);

		stateChangedAt = curTick;
	}
}

void
ITCA::ITCAAccountingState::handleSampleTransition(Tick sampleSize){
	fatal("sample transitions not implemented");
	assert(accountedCycles + notAccountedCycles == sampleSize);
}

void
ITCA::ITCAAccountingState::reset(){
	accountedCycles = 0;
	notAccountedCycles = 0;

	fatal("Reset needs to update accounting and stateChangedAt");
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

	DPRINTF(ITCA, "CPU %d: Removed element at position %d, addr %d, new size is %d\n",
			cpuID,
		 	foundAt,
		 	addr,
		 	table->size());
}
