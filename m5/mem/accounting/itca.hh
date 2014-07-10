/*
 * itca.hh
 *
 *  Created on: Jul 9, 2014
 *      Author: jahre
 */

#ifndef ITCA_HH_
#define ITCA_HH_

#include "sim/sim_object.hh"
#include "base/misc.hh"
#include "base/trace.hh"
#include <iostream>

class ITCA{
public:
	enum ITCASignals{
		ITCA_IT_INSTRUCTION,
		ITCA_ROB_EMPTY,
		ITCA_INTER_TOP_ROB,
		ITCA_RENAME_STALLED,
		ITCA_ALL_MSHRS_INTER,
		ITCA_SIGNAL_CNT
	};

private:
	class ITCAAccountingState{
	public:
		bool accounting;
		Tick stateChangedAt;
		Tick accountedCycles;
		Tick notAccountedCycles;

		ITCAAccountingState();

		void update(bool stopAccounting);
		void reset();
		void handleSampleTransition(Tick sampleSize);
	};

	class ITCASignalState{
	public:
		std::vector<bool> signalOn;

		ITCASignalState(){
			signalOn.resize(ITCA_SIGNAL_CNT, false);
		}

		void set(ITCASignals signal);
		void unset(ITCASignals signal);
	};

	int cpuID;
	ITCAAccountingState accountingState;
	ITCASignalState signalState;

	void processSignalChange();

public:
	ITCA(int _cpuID);

	void setSignal(ITCASignals signal);

	void unsetSignal(ITCASignals signal);

	Tick getAccountedCycles(Tick sampleSize);

};


#endif /* ITCA_HH_ */
