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

class ITCA : public SimObject{
public:
	enum ITCASignals{
		ITCA_IT_INSTRUCTION,
		ITCA_ROB_EMPTY,
		ITCA_INTER_TOP_ROB,
		ITCA_CPU_STALLED,
		ITCA_ALL_MSHRS_INTER,
		ITCA_SIGNAL_CNT
	};

	enum ITCACPUStalls{
		ITCA_DISPATCH_STALL,
		ITCA_REG_RENAME_STALL,
		ITCA_COMMIT_STALL,
		ITCA_CPU_STALL_CNT
	};

	enum ITCAInterTaskInstructionPolicy{
		ITCA_ITIP_ONE,
		ITCA_ITIP_ALL,
		ITCA_ITPI_CNT
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
		void clear(ITCASignals signal);
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
	ITCACPUStalls useCPUStallSignal;
	ITCAInterTaskInstructionPolicy useITIP;
	Addr headOfROBAddr;

	std::vector<ITCATableEntry> dataMissTable;
	std::vector<ITCATableEntry> instructionMissTable;

	static char *cpuStallSignalNames[ITCA_CPU_STALL_CNT];
	static char *mainSignalNames[ITCA_SIGNAL_CNT];

	void processSignalChange();

	void checkAllMSHRsInterSig();

	void removeTableEntry(std::vector<ITCATableEntry>* table, Addr addr, bool acceptNotFound = false);

	int findTableEntry(std::vector<ITCATableEntry>* table, Addr addr, bool acceptNotFound = false);

	void updateInterTopROB();

	void updateInterTaskInstruction();

	void runITCALogic();

public:
	ITCA(std::string _name, int _cpuID, ITCACPUStalls _cpuStall, ITCAInterTaskInstructionPolicy _itip, bool _doVerification);

	Tick getAccountedCycles();

	void l1DataMiss(Addr addr);
	void l1InstructionMiss(Addr addr);
	void squash(Addr addr);
	void l1MissResolved(Addr addr, Tick willFinishAt, bool isDataMiss);
	void handleL1MissResolvedEvent(Addr addr, bool isDataMiss);

	void intertaskMiss(Addr addr, bool isInstructionMiss);

	void itcaCPUStalled(ITCACPUStalls type);
	void itcaCPUResumed(ITCACPUStalls type);

	void setROBHeadAddr(Addr addr);
	void clearROBHeadAddr();

	void setROBEmpty();
	void clearROBEmpty();

	void testSignals();
};


class ITCAMemoryRequestCompletionEvent : public Event{
    ITCA* itca;
	Addr addr;
	bool isDataMiss;

    public:
    // constructor
    /** A simple constructor. */
	ITCAMemoryRequestCompletionEvent (ITCA* _itca, Addr _addr, bool _isDataMiss)
        : Event(&mainEventQueue), itca(_itca), addr(_addr), isDataMiss(_isDataMiss)
    {
    }

    // event execution function
    /** Calls BusInterface::deliver() */
    void process(){
        itca->handleL1MissResolvedEvent(addr, isDataMiss);
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

class ITCATestEvent: public Event{
    ITCA* itca;

    public:
    // constructor
    /** A simple constructor. */
	ITCATestEvent (ITCA* _itca)
        : Event(&mainEventQueue), itca(_itca)
    {
    }

    // event execution function
    /** Calls BusInterface::deliver() */
    void process(){
        itca->testSignals();
        delete this;
    }

    /**
    * Returns the string description of this event.
    * @return The description of this event.
     */
    virtual const char *description(){
        return "ITCA Test Event";
    }
};

#endif /* ITCA_HH_ */
