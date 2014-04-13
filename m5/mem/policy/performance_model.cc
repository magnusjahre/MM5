/*
 * performance_model.cc
 *
 *  Created on: Apr 13, 2014
 *      Author: jahre
 */

#include "performance_model.hh"

using namespace std;

PerformanceModel::PerformanceModel(int _cpuID){
	cpuID = _cpuID;
	reset();
	initModelTrace(_cpuID);

	cummulativeInstructions = 0;
}

void
PerformanceModel::reset(){
	CPIMemInd = -1.0;
	CPIOther = -1.0;
	CPIPrivLoads = -1.0;
	CPIOverlap = -1.0;
	CPINoBus = -1.0;
	alpha = -1.0;
	missMax = -1.0;
	missMin = -1.0;
	wayThreshold = -1.0;
	instructions = -1.0;
	CPL = -1.0;
	ticksInSample = -1.0;
	measuredCPIBus = -1.0;
}

bool
PerformanceModel::isValid(){
	return CPIMemInd > 0.0
			&& CPIOther >= 0.0
			&& CPIPrivLoads >= 0.0
			&& CPIOverlap >= 0.0
			&& CPINoBus >= 0.0
			&& alpha >= 0.0
			&& missMax >= 0.0
			&& missMin >= 0.0
			&& wayThreshold > 0.0
			&& instructions > 0.0
	        && CPL >= 0.0
			&& ticksInSample > 0.0
			&& measuredCPIBus >= 0.0;
}

void
PerformanceModel::setHelperVariables(double _instructions, double _CPL, double _ticksInSample){
	instructions = _instructions;
	CPL = _CPL;
	ticksInSample = _ticksInSample;

	cummulativeInstructions += _instructions;
}

void
PerformanceModel::updateCPIMemInd(double commitCycles, double memIndStallCycles){
	CPIMemInd = (commitCycles + memIndStallCycles) / instructions;
}

void
PerformanceModel::updateCPIOther(double stallWrite, double stallBlocked, double stallEmptyROB){
	CPIOther = (stallWrite + stallBlocked + stallEmptyROB) / instructions;
}

void
PerformanceModel::updateCPIPrivLoads(double stallPrivateLoads){
	CPIPrivLoads = stallPrivateLoads / instructions;
}

void
PerformanceModel::updateCPIOverlap(double overlap){
	CPIOverlap = (CPL * overlap) / instructions;
}

void
PerformanceModel::updateCPINoBus(double avgNoBusLat){
	CPINoBus = (CPL * avgNoBusLat) / instructions;
}

void
PerformanceModel::updateMeasuredCPIBus(double avgBusLat){
	measuredCPIBus = (CPL * avgBusLat) / instructions;
}

double
PerformanceModel::computeCPINoShare(){
	return CPIMemInd + CPIOther + CPIPrivLoads - CPIOverlap + CPINoBus;
}

void
PerformanceModel::calibrateBusModel(){
	fatal("not implemented");
}

void
PerformanceModel::calibrateCacheModel(){
	fatal("not implemented");
}

double
PerformanceModel::estimatePerformance(double ways, double bwShare){
	assert(isValid());
	fatal("not implemented");
	return 0.0;
}

void
PerformanceModel::initModelTrace(int cpuID){

	modelTrace = RequestTrace("PerformanceModel", RequestTrace::buildFilename("ModelTrace", cpuID).c_str());
	vector<string> headers;
	headers.push_back("Cum. Com. Insts");
	headers.push_back("Actual CPI");
	headers.push_back("CPI No Share (agg.)");
	headers.push_back("Memory Independent CPI");
	headers.push_back("Other CPI");
	headers.push_back("Private Load CPI");
	headers.push_back("Overlap CPI");
	headers.push_back("No Bus CPI");
	headers.push_back("Measured Bus CPI");

	modelTrace.initalizeTrace(headers);
}

void
PerformanceModel::traceModelParameters(){
	vector<RequestTraceEntry> data;
	data.push_back(cummulativeInstructions);
	data.push_back(ticksInSample / instructions);
	data.push_back(computeCPINoShare());
	data.push_back(CPIMemInd);
	data.push_back(CPIOther);
	data.push_back(CPIPrivLoads);
	data.push_back(CPIOverlap);
	data.push_back(CPINoBus);
	data.push_back(measuredCPIBus);

	modelTrace.addTrace(data);
}



