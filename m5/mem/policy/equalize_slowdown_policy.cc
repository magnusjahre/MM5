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
											   string _searchAlgorithm)
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
	maxWays = 0;

	if(_searchAlgorithm == "exhaustive"){
		searchAlgorithm = ESP_SEARCH_EXHAUSTIVE;
	}
	else if(_searchAlgorithm == "lookahead"){
		searchAlgorithm = ESP_SEARCH_LOOKAHEAD;
	}
	else{
		fatal("Unknown search algorithm provided");
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
EqualizeSlowdownPolicy::init(){
	if(cpuCount != 1){
		disableCommitSampling();
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
	double cpl = sharedCPLMeasurements[cpuID];

	DPRINTF(MissBWPolicy, "Constant b for CPU %d: non stall cycles %f, pre LLC avg latency %f and private memsys %f (total %f), cpl %f\n",
			cpuID,
			nonSharedCycles,
			sharedPreLLCAvgLatencies[cpuID],
			privateMemsysAvgLatency[cpuID],
			preLLCAvgLat,
			cpl);

	double cyclesInfLLC = nonSharedCycles + (cpl*preLLCAvgLat);
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
EqualizeSlowdownPolicy::computeGradientForCPU(PerformanceMeasurement measurement, int cpuID, double b){
	vector<double> measuredCPIs = measurement.getSharedModeCPIs();
	int llcMisses = measurement.perCoreCacheMeasurements[cpuID].readMisses;
	double sharedMemsysCPIcomp = measuredCPIs[cpuID] - b;

	if(llcMisses == 0){
		DPRINTF(MissBWPolicy, "Gradient for CPU %d is 0 because there no LLC misses\n", cpuID);
		return 0.0;
	}
	assert(llcMisses > 0);

	if(sharedMemsysCPIcomp <= 0.0){
		DPRINTF(MissBWPolicy, "Gradient for CPU %d is 0 because the estimated shared memsys CPI component is %f (<= 0)\n", cpuID, sharedMemsysCPIcomp);
		return 0.0;
	}

	double gradient = sharedMemsysCPIcomp / (double) llcMisses;
	assert(gradient >= 0.0);

	DPRINTF(MissBWPolicy, "Gradient for CPU %d: computed gradient %f with CPI %f, b %f and misses %d\n",
			cpuID,
			gradient,
			measuredCPIs[cpuID],
			b,
			llcMisses);

	return gradient;
}

double
EqualizeSlowdownPolicy::computeSpeedup(int cpuID, int misses, double gradient, double b){
	double estimatedCPI = b;
	if(misses > 0){
		estimatedCPI = misses * gradient + b;
		DPRINTF(MissBWPolicyExtra, "--- CPU %d: CPI(%d) = %f * m + %f = %f\n", cpuID, misses, gradient, b, estimatedCPI);
	}
	else{
		DPRINTF(MissBWPolicyExtra, "--- CPU %d: CPI(%d) = %f\n", cpuID, misses, b);
	}

	assert(b >= 0);
	double estimatedIPC = 1/estimatedCPI;
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
	bestAllocation = sharedCaches[0]->lookaheadCachePartitioning(speedups);

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
	for(int i=0;i<cpuCount;i++){
		gradients[i] = computeGradientForCPU(measurements, i, constBs[i]);
		DPRINTF(MissBWPolicy, "CPU %d: CPI(m) = %f * m + %f\n", i, gradients[i], constBs[i]);
	}

	dumpMissCurves(measurements);

	assert(!sharedCaches.empty());
	maxWays = sharedCaches[0]->getAssoc();
	bestMetricValue = 0.0;
	bestAllocation = vector<int>(cpuCount, 0.0);

	if(searchAlgorithm == ESP_SEARCH_EXHAUSTIVE){
		exhaustiveSearch(&measurements, vector<int>(), gradients, constBs);
	}
	else if(searchAlgorithm == ESP_SEARCH_LOOKAHEAD){
		lookaheadSearch(&measurements, gradients, constBs);
	}

	DPRINTF(MissBWPolicy, "Found best allocation %swith metric value %f for metric %s\n",
			              getAllocString(bestAllocation).c_str(),
						  bestMetricValue,
						  performanceMetric->metricName());

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
	INIT_PARAM_DFLT(searchAlgorithm, "The algorithm to use to find the cache partition", "exhaustive")
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
									  searchAlgorithm);
}

REGISTER_SIM_OBJECT("EqualizeSlowdownPolicy", EqualizeSlowdownPolicy)

#endif //DOXYGEN_SHOULD_SKIP_THIS

