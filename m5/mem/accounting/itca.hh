/*
 * itca.hh
 *
 *  Created on: Jul 9, 2014
 *      Author: jahre
 */

#ifndef ITCA_HH_
#define ITCA_HH_

#include "sim/sim_object.hh"
#include "sim/eventq.hh"
#include "mem/base_hier.hh"
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
		int cpuID;

		ITCAAccountingState();
		void setCPUID(int _cpuID) { cpuID = _cpuID; }

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

	class ITCATableEntry{
	public:
		Addr addr;
		bool intertaskMiss;

		ITCATableEntry() : addr(0), intertaskMiss(false) {}
		ITCATableEntry(Addr _addr) : addr(_addr), intertaskMiss(false) {}
	};

	int cpuID;
	ITCAAccountingState accountingState;
	ITCASignalState signalState;
	Tick lastSampleAt;

	std::vector<ITCATableEntry> dataMissTable;
	std::vector<ITCATableEntry> instructionMissTable;

	void processSignalChange();

	void checkAllMSHRsInterSig();

	void removeTableEntry(std::vector<ITCATableEntry>* table, Addr addr);

	int findTableEntry(std::vector<ITCATableEntry>* table, Addr addr);

public:
	ITCA(int _cpuID);

	void setSignal(ITCASignals signal);

	void unsetSignal(ITCASignals signal);

	Tick getAccountedCycles();

	void l1DataMiss(Addr addr);

	void l1MissResolved(Addr addr, Tick willFinishAt);

	void handleL1MissResolvedEvent(Addr addr);

	void intertaskMiss(Addr addr, bool isInstructionMiss);

};


class ITCAMemoryRequestCompletionEvent : public Event{
    ITCA* itca;
	Addr addr;

    public:
    // constructor
    /** A simple constructor. */
	ITCAMemoryRequestCompletionEvent (ITCA* _itca, Addr _addr)
        : Event(&mainEventQueue), itca(_itca), addr(_addr)
    {
    }

    // event execution function
    /** Calls BusInterface::deliver() */
    void process(){
        itca->handleL1MissResolvedEvent(addr);
        delete this;
    }

    /**
    * Returns the string description of this event.
    * @return The description of this event.
     */
    virtual const char *description(){
        return "ITCA Memory Request Completion Event";
    }
};

#endif /* ITCA_HH_ */
