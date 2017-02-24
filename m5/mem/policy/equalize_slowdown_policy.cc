/*
 * equalize_slowdown.cc
 *
 *  Created on: Jan 15, 2016
 *      Author: jahre
 */

#include "equalize_slowdown_policy.hh"

using namespace std;

EqualizeSlowdownPolicy::EqualizeSlowdownPolicy(std::string _name,
										 	   InterferenceManager* _intManager,
											   Tick _period,
											   int _cpuCount,
											   PerformanceEstimationMethod _perfEstMethod,
											   bool _persistentAllocations,
											   int _iterationLatency,
											   Metric* _performanceMetric,
											   bool _enforcePolicy,
											   WriteStallTechnique _wst,
											   PrivBlockedStallTechnique _pbst,
											   EmptyROBStallTechnique _rst,
											   double _maximumDamping,
											   double _hybridDecisionError,
											   int _hybridBufferSize,
											   string _searchAlgorithm,
											   int _maxSteps,
											   std::string _gradientModel,
											   int _lookaheadCap)
: BasePolicy(_name,
			_intManager,
			_period,
			_cpuCount,
			_perfEstMethod,
			_persistentAllocations,
			_iterationLatency,
			_performanceMetric,
			_enforcePolicy,
			_wst,
			_pbst,
			_rst,
			_maximumDamping,
			_hybridDecisionError,
			_hybridBufferSize){

	bestMetricValue = 0.0;
	bestAllocation = vector<int>(cpuCount, 0.0);
	espLocalOverlap = vector<double>(cpuCount, 0.0);
	maxWays = 0;
	maxSteps = _maxSteps;
	lookaheadCap = _lookaheadCap;

	if(_searchAlgorithm == "exhaustive"){
		searchAlgorithm = ESP_SEARCH_EXHAUSTIVE;
	}
	else if(_searchAlgorithm == "lookahead"){
		searchAlgorithm = ESP_SEARCH_LOOKAHEAD;
	}
	else{
		fatal("Unknown search algorithm provided");
	}

	if(_gradientModel == "computed"){
		gradientModel = ESP_GRADIENT_COMPUTED;
	}
	else if(_gradientModel == "global"){
		gradientModel = ESP_GRADIENT_GLOBAL;
	}
	else{
		fatal("Unknown gradient model");
	}

	allocationTrace = RequestTrace(_name, "AllocationTrace");
	vector<string> header = vector<string>();
	for(int i=0;i<_cpuCount;i++){
		stringstream curstr;
		curstr << "CPU" << i;
		header.push_back(curstr.str());
	}
	allocationTrace.initalizeTrace(header);
}

void
EqualizeSlowdownPolicy::initPolicy(){
	if(cpuCount != 1){
		disableCommitSampling();
	}

	assert(!sharedCaches.empty());
	maxWays = sharedCaches[0]->getAssoc();

	initCurveTracefiles();
}

void
EqualizeSlowdownPolicy::initCurveTracefiles(){
	vector<string> header;
	for(int i=0;i<maxWays;i++){
		stringstream head;
		head << (i+1);
		if(i == 0) head << " way";
		else head << " ways";
		header.push_back(head.str());
	}

	missCurveTraces.resize(cpuCount, RequestTrace());
	performanceCurveTraces.resize(cpuCount, RequestTrace());
	speedupCurveTraces.resize(cpuCount, RequestTrace());

	for(int i=0;i<cpuCount;i++){
		stringstream filename;
		filename << "MissCurveTrace" << i;
		missCurveTraces[i] = RequestTrace(name(), filename.str().c_str());
		missCurveTraces[i].initalizeTrace(header);

		filename.str("");
		filename << "PerformanceCurveTrace" << i;
		performanceCurveTraces[i] = RequestTrace(name(), filename.str().c_str());
		performanceCurveTraces[i].initalizeTrace(header);

		filename.str("");
		filename << "SpeedupCurveTrace" << i;
		speedupCurveTraces[i] = RequestTrace(name(), filename.str().c_str());
		speedupCurveTraces[i].initalizeTrace(header);
	}
}

template<typename T>
std::vector<RequestTraceEntry>
EqualizeSlowdownPolicy::getTraceCurve(std::vector<T> data){
	vector<RequestTraceEntry> reqData;
	for(int i=0;i<data.size();i++){
		reqData.push_back(RequestTraceEntry(data[i]));
	}
	return reqData;
}


void
EqualizeSlowdownPolicy::traceCurves(PerformanceMeasurement* measurements,
        							std::vector<double> gradients,
									std::vector<double> bs){

	for(int i=0;i<cpuCount;i++){
		vector<int> misses = measurements->perCoreCacheMeasurements[i].privateCumulativeCacheMisses;
		vector<RequestTraceEntry> line = getTraceCurve(misses);
		missCurveTraces[i].addTrace(line);

		vector<double> perfEstimates = vector<double>(maxWays, 0.0);
		vector<double> speedups = vector<double>(maxWays, 0.0);
		for(int j=0;j<maxWays;j++){
			perfEstimates[j] = computeIPC(i, misses[j], gradients[i], bs[i]);
			speedups[j] = computeSpeedup(i, misses[j], gradients[i], bs[i]);
		}

		vector<RequestTraceEntry> perfLine = getTraceCurve(perfEstimates);
		performanceCurveTraces[i].addTrace(perfLine);

		vector<RequestTraceEntry> speedupLine = getTraceCurve(speedups);
		speedupCurveTraces[i].addTrace(speedupLine);
	}

}

double
EqualizeSlowdownPolicy::getConstBForCPU(PerformanceMeasurement measurements, int cpuID){
	// cpuSharedStallCycles contain the shared stall cycles. Thus, subtracting this from the period
	// gives the sum of compute cycles, memory independent stalls and other stalls (empty ROB, blocked
	// private memory system and blocked store queue).
	double nonSharedCycles = sharedNonSharedLoadCycles[cpuID];
	vector<double> sharedPreLLCAvgLatencies = measurements.getSharedPreLLCAvgLatencies();
	double preLLCAvgLat = sharedPreLLCAvgLatencies[cpuID] + privateMemsysAvgLatency[cpuID];

	espLocalOverlap[cpuID] = 0.0;
	double requests = measurements.requestsInSample[cpuID];
	assert(requests >= 0);
	assert(measurements.sharedLatencies[cpuID] >= 0);
	if(requests > 0){
		assert(sharedLoadStallCycles[cpuID] >= 0);
		espLocalOverlap[cpuID]  = sharedLoadStallCycles[cpuID] / (requests * measurements.sharedLatencies[cpuID]);
	}

	DPRINTF(MissBWPolicy, "Constant b for CPU %d: non stall cycles %f, pre LLC avg latency %f and private memsys %f (total %f), overlap %f, requests %d\n",
			cpuID,
			nonSharedCycles,
			sharedPreLLCAvgLatencies[cpuID],
			privateMemsysAvgLatency[cpuID],
			preLLCAvgLat,
			espLocalOverlap[cpuID],
			requests);

	double cyclesInfLLC = nonSharedCycles + (espLocalOverlap[cpuID] *requests*preLLCAvgLat);
	double CPIInfLLC = cyclesInfLLC / measurements.committedInstructions[cpuID];
	assert(CPIInfLLC > 0.0);

	DPRINTF(MissBWPolicy, "Constant b for CPU %d: cycles inf LLC %f, committed instructions %d, CPI inf LLC %f\n",
				cpuID,
				cyclesInfLLC,
				measurements.committedInstructions[cpuID],
				CPIInfLLC);

	return CPIInfLLC;
}

double
EqualizeSlowdownPolicy::computeGradientForCPU(PerformanceMeasurement measurement, int cpuID, double b, double avgMemBusLat){


	double gradient = 0.0;
	if(gradientModel == ESP_GRADIENT_COMPUTED){

		vector<double> measuredCPIs = measurement.getSharedModeCPIs();
		int llcMisses = measurement.perCoreCacheMeasurements[cpuID].readMisses;
		double sharedMemsysCPIcomp = measuredCPIs[cpuID] - b;

		if(llcMisses > 0 && sharedMemsysCPIcomp > 0.0){
			assert(llcMisses > 0);
			assert(sharedMemsysCPIcomp > 0.0);
			gradient = sharedMemsysCPIcomp / (double) llcMisses;

			DPRINTF(MissBWPolicy, "Gradient for CPU %d: computed gradient %f with CPI %f, b %f and misses %d\n",
				   				  cpuID,
								  gradient,
								  measuredCPIs[cpuID],
								  b,
								  llcMisses);
		}
		else{
			gradient = 0.0;
			DPRINTF(MissBWPolicy, "Gradient for CPU %d is 0 because either post LLC CPI component is %f (<= 0) or misses %d (== 0)\n",
					              cpuID,
								  sharedMemsysCPIcomp,
								  llcMisses);

		}
	}
	else if(gradientModel == ESP_GRADIENT_GLOBAL){
		assert(espLocalOverlap[cpuID] >= 0.0);
		double instructions = (double) measurement.committedInstructions[cpuID];

		gradient = (espLocalOverlap[cpuID] * avgMemBusLat) / instructions;

		DPRINTF(MissBWPolicy, "Gradient for CPU %d: computed gradient %f with instructions %f, average memory bus latency %f and overlap %f\n",
						      cpuID,
							  gradient,
							  instructions,
							  avgMemBusLat,
							  espLocalOverlap[cpuID]);
	}
	else{
		fatal("Unknown gradient model");
	}

	assert(gradient >= 0.0);
	return gradient;
}

double
EqualizeSlowdownPolicy::computeIPC(int cpuID, int misses, double gradient, double b){

	assert(misses >= 0);
	assert(b >= 0.0);
	double estimatedCPI = (misses * gradient) + b;
	DPRINTF(MissBWPolicyExtra, "--- CPU %d: CPI(%d) = %f * m + %f = %f\n", cpuID, misses, gradient, b, estimatedCPI);

	assert(estimatedCPI >= b);
	double estimatedIPC = 1/estimatedCPI;
	DPRINTF(MissBWPolicyExtra, "--- CPU %d: IPC = %f\n", cpuID, estimatedIPC);

	return estimatedIPC;
}

double
EqualizeSlowdownPolicy::computeSpeedup(int cpuID, int misses, double gradient, double b){
	double estimatedIPC = computeIPC(cpuID, misses, gradient, b);
	double speedup = estimatedIPC / aloneIPCEstimates[cpuID];
	DPRINTF(MissBWPolicyExtra, "--- CPU %d: Speedup = %f / %f = %f\n", cpuID, estimatedIPC, aloneIPCEstimates[cpuID], speedup);
	return speedup;
}

std::vector<double>
EqualizeSlowdownPolicy::computeSpeedups(PerformanceMeasurement* measurements,
		                                std::vector<int> allocation,
										std::vector<double> gradients,
										std::vector<double> bs){

	vector<double> speedups = vector<double>(cpuCount, 0.0);
	assert(allocation.size() == cpuCount);
	assert(gradients.size() == cpuCount);
	assert(bs.size() == cpuCount);

	for(int i=0;i<speedups.size();i++){
		int misses = measurements->perCoreCacheMeasurements[i].privateCumulativeCacheMisses[allocation[i]-1];
		speedups[i] = computeSpeedup(i, misses, gradients[i], bs[i]);
		DPRINTF(MissBWPolicyExtra, "--- CPU %d: Speedup %f with allocation %d (index %d)\n", i, speedups[i], allocation[i], allocation[i]-1);
	}
	return speedups;
}

void
EqualizeSlowdownPolicy::lookaheadSearch(PerformanceMeasurement* measurements,
		                                std::vector<double> gradients,
										std::vector<double> bs){

	vector<vector<double> > speedups(cpuCount, vector<double>(maxWays, 0.0));
	for(int i=0;i<speedups.size();i++){
		for(int j=0;j<maxWays;j++){
			int misses = measurements->perCoreCacheMeasurements[i].privateCumulativeCacheMisses[j];
			speedups[i][j] = computeSpeedup(i, misses, gradients[i], bs[i]);
		}
	}

	assert(!sharedCaches.empty());
	bestAllocation = sharedCaches[0]->lookaheadCachePartitioning(speedups, lookaheadCap);

	vector<double> bestAllocSpeedups = vector<double>(cpuCount, 0.0);
	for(int i=0;i<cpuCount;i++){
		bestAllocSpeedups[i] = speedups[i][bestAllocation[i]-1];
	}

	bestMetricValue = performanceMetric->computeMetric(&bestAllocSpeedups, NULL);
}

void
EqualizeSlowdownPolicy::runPolicy(PerformanceMeasurement measurements){

	DPRINTF(MissBWPolicy, "Running performance-based cache partitioning policy\n");

	vector<double> constBs(cpuCount, 0.0);
	for(int i=0;i<cpuCount;i++){
		constBs[i] = getConstBForCPU(measurements, i);
	}

	vector<double> gradients(cpuCount, 0.0);
	double avgMemBusLat = measurements.getGlobalAvgMemBusLatency();
	for(int i=0;i<cpuCount;i++){
		gradients[i] = computeGradientForCPU(measurements, i, constBs[i], avgMemBusLat);
		DPRINTF(MissBWPolicy, "CPU %d: CPI(m) = %f * m + %f\n", i, gradients[i], constBs[i]);
	}

	dumpMissCurves(measurements);
	traceCurves(&measurements, gradients, constBs);

	assert(maxWays != 0);
	bestMetricValue = 0.0;
	vector<int> currentAllocation = bestAllocation;
	bestAllocation = vector<int>(cpuCount, 0.0);

	if(searchAlgorithm == ESP_SEARCH_EXHAUSTIVE){
		exhaustiveSearch(&measurements, vector<int>(), gradients, constBs);
	}
	else if(searchAlgorithm == ESP_SEARCH_LOOKAHEAD){
		lookaheadSearch(&measurements, gradients, constBs);
	}

	DPRINTF(MissBWPolicy, "Found best allocation %swith metric value %f for metric %s, current allocation %s\n",
			              getAllocString(bestAllocation).c_str(),
						  bestMetricValue,
						  performanceMetric->metricName(),
						  getAllocString(currentAllocation).c_str());
	assert(sum(bestAllocation) == maxWays);

	assert(!sharedCaches.empty());
	bestAllocation = sharedCaches[0]->findAllocation(currentAllocation, bestAllocation, maxSteps);

	DPRINTF(MissBWPolicy, "Implemented allocation %swith max steps %d\n",
				          getAllocString(bestAllocation).c_str(),
						  maxSteps);
	assert(sum(bestAllocation) == maxWays);

	vector<RequestTraceEntry> tracedata = vector<RequestTraceEntry>();
	for(int i=0;i<bestAllocation.size();i++) tracedata.push_back(bestAllocation[i]);
	allocationTrace.addTrace(tracedata);

	for(int i=0;i<sharedCaches.size();i++){
		sharedCaches[i]->setCachePartition(bestAllocation);
		sharedCaches[i]->enablePartitioning();
	}
}

void
EqualizeSlowdownPolicy::evaluateAllocation(PerformanceMeasurement* measurements,
		                                   std::vector<int> allocation,
										   std::vector<double> gradients,
										   std::vector<double> bs){
	vector<double> speedups = computeSpeedups(measurements, allocation, gradients, bs);
	double metricValue = performanceMetric->computeMetric(&speedups, NULL);
	assert(metricValue > 0.0);

	if(metricValue > bestMetricValue){
		DPRINTF(MissBWPolicyExtra, "Found new best allocation %swith new metric value %f, old %f\n",
				getAllocString(allocation).c_str(),
				bestMetricValue,
				metricValue);

		bestMetricValue = metricValue;
		bestAllocation = allocation;
	}
}

void
EqualizeSlowdownPolicy::exhaustiveSearch(PerformanceMeasurement* measurements,
                                         std::vector<int> allocation,
										 std::vector<double> gradients,
										 std::vector<double> bs){
	if(allocation.size() < cpuCount){
		if(sum(allocation) > maxWays) return;
		for(int i=1;i<maxWays;i++){
			vector<int> newalloc = vector<int>(allocation);
			newalloc.push_back(i);
			exhaustiveSearch(measurements, newalloc, gradients, bs);
		}
	}
	else{
		assert(allocation.size() == cpuCount);
		if(sum(allocation) == maxWays){
			evaluateAllocation(measurements, allocation, gradients, bs);
		}
	}
}

int
EqualizeSlowdownPolicy::sum(std::vector<int> allocation){
	int sum = 0;
	for(int i=0;i<allocation.size();i++){
		sum += allocation[i];
	}
	return sum;
}

std::string
EqualizeSlowdownPolicy::getAllocString(std::vector<int> allocation){
	stringstream retstr;
	for(int i=0;i<allocation.size();i++){
		retstr << i << ":" << allocation[i] << " ";
	}
	return retstr.str();
}

void
EqualizeSlowdownPolicy::dumpMissCurves(PerformanceMeasurement measurements){
	for(int i=0;i<measurements.perCoreCacheMeasurements.size();i++){
		DPRINTF(MissBWPolicy, "CPU %d Miss Curve", i);
		for(int j=0;j<measurements.perCoreCacheMeasurements[i].privateCumulativeCacheMisses.size();j++){
			DPRINTFR(MissBWPolicy, ";%d", measurements.perCoreCacheMeasurements[i].privateCumulativeCacheMisses[j]);
		}
		DPRINTFR(MissBWPolicy, "\n");
	}
}

bool
EqualizeSlowdownPolicy::doEvaluation(int cpuID){
	return true;
}

#ifndef DOXYGEN_SHOULD_SKIP_THIS

BEGIN_DECLARE_SIM_OBJECT_PARAMS(EqualizeSlowdownPolicy)
	SimObjectParam<InterferenceManager* > interferenceManager;
	Param<Tick> period;
	Param<int> cpuCount;
	Param<string> performanceEstimationMethod;
	Param<bool> persistentAllocations;
	Param<int> iterationLatency;
	Param<string> optimizationMetric;
	Param<bool> enforcePolicy;
	Param<string> writeStallTechnique;
	Param<string> privateBlockedStallTechnique;
	Param<string> emptyROBStallTechnique;
	Param<double> maximumDamping;
	Param<double> hybridDecisionError;
	Param<int> hybridBufferSize;
	Param<string> searchAlgorithm;
	Param<int> maxSteps;
	Param<string> gradientModel;
	Param<int> lookaheadCap;
END_DECLARE_SIM_OBJECT_PARAMS(EqualizeSlowdownPolicy)

BEGIN_INIT_SIM_OBJECT_PARAMS(EqualizeSlowdownPolicy)
	INIT_PARAM_DFLT(interferenceManager, "The system's interference manager" , NULL),
	INIT_PARAM_DFLT(period, "The number of clock cycles between each decision", 1048576),
	INIT_PARAM(cpuCount, "The number of cpus in the system"),
	INIT_PARAM(performanceEstimationMethod, "The method to use for performance estimations"),
	INIT_PARAM_DFLT(persistentAllocations, "The method to use for performance estimations", true),
	INIT_PARAM_DFLT(iterationLatency, "The number of cycles it takes to evaluate one MHA", 0),
	INIT_PARAM_DFLT(optimizationMetric, "The metric to optimize for", "stp"),
	INIT_PARAM_DFLT(enforcePolicy, "Should the policy be enforced?", true),
	INIT_PARAM(writeStallTechnique, "The technique to use to estimate private write stalls"),
	INIT_PARAM(privateBlockedStallTechnique, "The technique to use to estimate private blocked stalls"),
	INIT_PARAM(emptyROBStallTechnique, "The technique to use to estimate private mode empty ROB stalls"),
	INIT_PARAM_DFLT(maximumDamping, "The maximum absolute damping the damping policies can apply", 0.25),
	INIT_PARAM_DFLT(hybridDecisionError, "The error at which to switch from CPL to CPL-CWP with the hybrid scheme", 0.0),
	INIT_PARAM_DFLT(hybridBufferSize, "The number of errors to use in the decision buffer", 3),
	INIT_PARAM_DFLT(searchAlgorithm, "The algorithm to use to find the cache partition", "exhaustive"),
	INIT_PARAM_DFLT(maxSteps, "Maximum number of changes from current allocation", 0),
	INIT_PARAM(gradientModel, "The model to use to estimate the LLC miss gradient"),
	INIT_PARAM_DFLT(lookaheadCap, "The maximum allocation in each round for the lookahead algorithm (0 == associativity == no cap)", 0)
END_INIT_SIM_OBJECT_PARAMS(EqualizeSlowdownPolicy)

CREATE_SIM_OBJECT(EqualizeSlowdownPolicy)
{
	BasePolicy::PerformanceEstimationMethod perfEstMethod =
		BasePolicy::parsePerformanceMethod(performanceEstimationMethod);

	Metric* performanceMetric = BasePolicy::parseOptimizationMetric(optimizationMetric);

	BasePolicy::WriteStallTechnique wst = BasePolicy::parseWriteStallTech(writeStallTechnique);
	BasePolicy::PrivBlockedStallTechnique pbst = BasePolicy::parsePrivBlockedStallTech(privateBlockedStallTechnique);

	BasePolicy::EmptyROBStallTechnique rst = BasePolicy::parseEmptyROBStallTech(emptyROBStallTechnique);

	return new EqualizeSlowdownPolicy(getInstanceName(),
							          interferenceManager,
									  period,
									  cpuCount,
									  perfEstMethod,
									  persistentAllocations,
									  iterationLatency,
									  performanceMetric,
									  enforcePolicy,
									  wst,
									  pbst,
									  rst,
									  maximumDamping,
									  hybridDecisionError,
									  hybridBufferSize,
									  searchAlgorithm,
									  maxSteps,
									  gradientModel,
									  lookaheadCap);
}

REGISTER_SIM_OBJECT("EqualizeSlowdownPolicy", EqualizeSlowdownPolicy)

#endif //DOXYGEN_SHOULD_SKIP_THIS

