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
											   int _hybridBufferSize)
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


	optimizationMetric = new STPPolicy(); //TODO: Parameterize
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
	double llcMisses = measurement.perCoreCacheMeasurements[cpuID].readMisses;
	double sharedMemsysCPIcomp = measuredCPIs[cpuID] - b;

	if(llcMisses == 0){
		DPRINTF(MissBWPolicy, "Gradient for CPU %d is 0 because there no LLC misses\n", cpuID);
		return 0.0;
	}

	double gradient = sharedMemsysCPIcomp / llcMisses;

	DPRINTF(MissBWPolicy, "Gradient for CPU %d: computed gradient %f with CPI %f, b %f and misses %f\n",
			cpuID,
			gradient,
			measuredCPIs[cpuID],
			b,
			llcMisses);

	return gradient;
}

//double
//EqualizeSlowdownPolicy::getGradientForCPU(PerformanceMeasurement measurements, int cpuID){
//	if(measurements.requestsInSample[cpuID] == 0){
//		DPRINTF(MissBWPolicy, "Gradient for CPU %d is 0 because there are 0 shared requests\n", cpuID);
//		return 0.0;
//	}
//
//	//FIXME: This value implicitly contains the current miss rate, needs to be accounted for
//	vector<double> sharedPreLLCAvgLatencies = measurements.getSharedPreLLCAvgLatencies();
//	double avgPostLLCLatency = sharedMemsysAvgLatency[cpuID] - sharedPreLLCAvgLatencies[cpuID];
//	assert(avgPostLLCLatency >= 0);
//
//	DPRINTF(MissBWPolicy, "Gradient for CPU %d: Post LLC avg latency %f from shared memsys avg latency %f and pre LLC avg latency %f for %d requests\n",
//			cpuID,
//			avgPostLLCLatency,
//			sharedMemsysAvgLatency[cpuID],
//			sharedPreLLCAvgLatencies[cpuID],
//			measurements.requestsInSample[cpuID]);
//
//	assert(measurements.committedInstructions[cpuID] > 0);
//	double gradient = (sharedCPLMeasurements[cpuID] * avgPostLLCLatency) / ((double)measurements.requestsInSample[cpuID] * (double) measurements.committedInstructions[cpuID]);
//
//	DPRINTF(MissBWPolicy, "Gradient for CPU %d is %f using cpl %f, requests %d and committed instructions %d\n",
//			cpuID,
//			gradient,
//			sharedCPLMeasurements[cpuID],
//			measurements.requestsInSample[cpuID],
//			measurements.committedInstructions[cpuID]);
//
//	return gradient;
//}

double
EqualizeSlowdownPolicy::computeSpeedup(int cpuID, int misses, double gradient, double b){
	double estimatedCPI = misses * gradient + b;
	DPRINTF(MissBWPolicyExtra, "--- CPU %d: CPI(%d) = %f * m + %f = %f\n", cpuID, misses, gradient, b, estimatedCPI);
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

	assert(!measurements.perCoreCacheMeasurements.empty());
	int maxWayIndex = measurements.perCoreCacheMeasurements[0].privateCumulativeCacheMisses.size();

	if(cpuCount != 4) fatal("Search method only supports 4 cores");

	vector<int> allocation = vector<int>(cpuCount, 0.0);
	vector<double> speedups = vector<double>(cpuCount, 0.0);
	double bestMetricValue = 0.0;
	vector<int> bestAllocation = vector<int>(cpuCount, 0.0);
	for(int a1=1;a1<maxWayIndex;a1++){
		allocation[0] = a1;
		for(int a2=1;a2<maxWayIndex;a2++){
			allocation[1] = a2;
			for(int a3=1;a3<maxWayIndex;a3++){
				allocation[2] = a3;
				for(int a4=1;a4<maxWayIndex;a4++){
					allocation[3] = a4;
					if(a1+a2+a3+a4 != 16) continue;

					speedups = computeSpeedups(&measurements, allocation, gradients, constBs);
					double metricValue = optimizationMetric->computeMetric(&speedups, NULL);

					if(metricValue > bestMetricValue){
						DPRINTF(MissBWPolicyExtra, "Found new best allocation %d,%d,%d,%d, new metric value %f, old %f\n",
								a1,a2,a3,a4,
								bestMetricValue,
								metricValue);

						bestMetricValue = metricValue;
						bestAllocation = allocation;
					}
				}
			}
		}
	}

	DPRINTF(MissBWPolicy, "Found best allocation %d,%d,%d,%d with metric value %f\n", bestAllocation[0], bestAllocation[1], bestAllocation[2], bestAllocation[3], bestMetricValue);

	/*vector<double> sharedModeIPCs = measurements.getSharedModeIPCs();
	for(int i=0;i<aloneIPCEstimates.size();i++){
		cout << "CPU" << i << ": " << aloneIPCEstimates[i] << " (pm) " << sharedModeIPCs[i] <<" (sm)\n";
	}

	for(int i=0;i<sharedCPLMeasurements.size();i++){
		cout << "CPU" << i << ": " << sharedCPLMeasurements[i] << "\n";
	}

	// sharedNonLoadCycles gives the non-shared-memory CPL component
    // sharedLoadStallCycles can be used for verification of the shared-memory CPL component

	vector<double> preLLCAvgLatencies = measurements.getPreLLCAvgLatencies();
	for(int i=0;i<preLLCAvgLatencies.size();i++){
		cout << "CPU" << i << ": " << preLLCAvgLatencies[i] << ", demand reads " << measurements.requestsInSample[i] << "\n";
	}*/

	fatal("stop here for now");
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
END_DECLARE_SIM_OBJECT_PARAMS(EqualizeSlowdownPolicy)

BEGIN_INIT_SIM_OBJECT_PARAMS(EqualizeSlowdownPolicy)
	INIT_PARAM_DFLT(interferenceManager, "The system's interference manager" , NULL),
	INIT_PARAM_DFLT(period, "The number of clock cycles between each decision", 1048576),
	INIT_PARAM(cpuCount, "The number of cpus in the system"),
	INIT_PARAM(performanceEstimationMethod, "The method to use for performance estimations"),
	INIT_PARAM_DFLT(persistentAllocations, "The method to use for performance estimations", true),
	INIT_PARAM_DFLT(iterationLatency, "The number of cycles it takes to evaluate one MHA", 0),
	INIT_PARAM_DFLT(optimizationMetric, "The metric to optimize for", "hmos"),
	INIT_PARAM_DFLT(enforcePolicy, "Should the policy be enforced?", true),
	INIT_PARAM(writeStallTechnique, "The technique to use to estimate private write stalls"),
	INIT_PARAM(privateBlockedStallTechnique, "The technique to use to estimate private blocked stalls"),
	INIT_PARAM(emptyROBStallTechnique, "The technique to use to estimate private mode empty ROB stalls"),
	INIT_PARAM_DFLT(maximumDamping, "The maximum absolute damping the damping policies can apply", 0.25),
	INIT_PARAM_DFLT(hybridDecisionError, "The error at which to switch from CPL to CPL-CWP with the hybrid scheme", 0.0),
	INIT_PARAM_DFLT(hybridBufferSize, "The number of errors to use in the decision buffer", 3)
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
									  hybridBufferSize);
}

REGISTER_SIM_OBJECT("EqualizeSlowdownPolicy", EqualizeSlowdownPolicy)

#endif //DOXYGEN_SHOULD_SKIP_THIS

