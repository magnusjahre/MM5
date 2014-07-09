

#include "itca.hh"

using namespace std;

ITCA::ITCA(int _cpuID){
	accountingState = ITCAAccountingState();
	signalState = ITCASignalState();
	cpuID = _cpuID;
}

void
ITCA::processSignalChange(){

	// This code implements Figure 5 from Luque et al. (2012)
	bool portOne = signalState.signalOn[ITCA_IT_INSTRUCTION] && signalState.signalOn[ITCA_ROB_EMPTY];
	bool portTwo = signalState.signalOn[ITCA_INTER_TOP_ROB] && signalState.signalOn[ITCA_RENAME_STALLED];
	//FIXME: drawn as a NOR gate, but OR makes more sense to me now...
	bool stopAccounting = portOne || portTwo || signalState.signalOn[ITCA_ALL_MSHRS_INTER];

	DPRINTF(ITCA, "SignalChange: Port 1 %s, Port 2 %s, Port 3 %s; Decision %s\n",
				portOne ? "on": "off",
				portTwo ? "on": "off",
				signalState.signalOn[ITCA_ALL_MSHRS_INTER] ? "on": "off",
				stopAccounting ? "on": "off");

	accountingState.update(stopAccounting);
	fatal("ITCA process signal change not tested");
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
	accounting = false;
	stateChangedAt = 0;

	accountedCycles = 0;
	notAccountedCycles = 0;
}

void
ITCA::ITCAAccountingState::update(bool stopAccounting){
	if(accounting && stopAccounting){
		accounting = false;
		fatal("Change from accounting to not accounting not impl");
	}
	else if(!accounting && !stopAccounting){
		accounting = true;
		fatal("Change from not accounting to accounting not impl");
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
