/*
 * performance_model.hh
 *
 *  Created on: Apr 13, 2014
 *      Author: jahre
 */

#ifndef PERFORMANCE_MODEL_HH_
#define PERFORMANCE_MODEL_HH_

#include "sim/sim_object.hh"
#include "base/misc.hh"
#include "base/statistics.hh"
#include "mem/requesttrace.hh"

class PerformanceModel{
private:
	int cpuID;
	RequestTrace modelTrace;

	// model components
	double CPIMemInd;
	double CPIOther;
	double CPIPrivLoads;
	double CPIOverlap;
	double CPINoBus;
	double alpha;
	double missMax;
	double missMin;
	double wayThreshold;

	// helper variables
	double instructions;
	double CPL;
	double ticksInSample;
	double measuredCPIBus;
	Tick cummulativeInstructions;

	double computeCPINoShare();

	void initModelTrace(int cpuID);

public:
	PerformanceModel(int cpuID);

	bool isValid();

	void reset();

	void setHelperVariables(double _instructions, double _CPL, double _ticksInSample);

	void updateCPIMemInd(double commitCycles, double memIndStallCycles);

	void updateCPIOther(double stallWrite, double stallBlocked, double stallEmptyROB);

	void updateCPIPrivLoads(double stallPrivateLoads);

	void updateCPIOverlap(double overlap);

	void updateCPINoBus(double avgNoBusLat);

	void updateMeasuredCPIBus(double avgBusLat);

	void updateCPIOverlap();

	void calibrateBusModel();

	void calibrateCacheModel();

	double estimatePerformance(double ways, double bwShare);

	void traceModelParameters();
};



#endif /* PERFORMANCE_MODEL_HH_ */
