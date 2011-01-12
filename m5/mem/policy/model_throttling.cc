/*
 * model_throttling.cc
 *
 *  Created on: Jan 9, 2011
 *      Author: jahre
 */

#include "model_throttling.hh"

ModelThrottlingPolicy::ModelThrottlingPolicy(std::string _name,
			   	    			 InterferenceManager* _intManager,
			   	    			 Tick _period,
			   	    			 int _cpuCount,
			   	    			 PerformanceEstimationMethod _perfEstMethod,
			   	    			 bool _persistentAllocations,
			   	    			 int _iterationLatency,
			   	    			 Metric* _performanceMetric,
			   	    			 bool _enforcePolicy)
: BasePolicy(_name, _intManager, _period, _cpuCount, _perfEstMethod, _persistentAllocations, _iterationLatency, _performanceMetric, _enforcePolicy)
{
	intManager->enableMSHROccupancyTrace();

	mshrOccupancyPtrs.resize(_cpuCount, NULL);
}

void
ModelThrottlingPolicy::runPolicy(PerformanceMeasurement measurements){

	DPRINTF(MissBWPolicy, "--- Running Model Throttling Policy\n");

	for(int i=0;i<mshrOccupancyPtrs.size();i++){
		assert(mshrOccupancyPtrs[i] == NULL);
		mshrOccupancyPtrs[i] = intManager->getMSHROccupancyList(i);
	}

	simpleSearch(&measurements);

	// clean up
	for(int i=0;i<mshrOccupancyPtrs.size();i++) mshrOccupancyPtrs[i] = NULL;
	intManager->clearMSHROccupancyLists();

	fatal("stop here for now");
}

bool
ModelThrottlingPolicy::doEvaluation(int cpuID){
	fatal("model throttling doEvaluation not implemented");
	return false;
}

void
ModelThrottlingPolicy::simpleSearch(PerformanceMeasurement* measurements){
	assert(cpuCount == 4);

	int maxMSHRs = 16;

	vector<int> throttling = vector<int>(4, 0);
	throttling[0] = 0;
	throttling[1] = 100;
	throttling[2] = 200;
	throttling[3] = 300;

	bestMetricValue = 0.0;

	for(int m1=1;m1<=maxMSHRs;m1 = m1 << 1){
		for(int m2=1;m2<=maxMSHRs;m2 = m2 << 1){
			for(int m3=1;m3<=maxMSHRs;m3 = m3 << 1){
				for(int m4=1;m4<=maxMSHRs;m4 = m4 << 1){
					for(int s1=0;s1<throttling.size();s1++){
						for(int s2=0;s2<throttling.size();s2++){
							for(int s3=0;s3<throttling.size();s3++){
								for(int s4=0;s4<throttling.size();s4++){
									vector<int> mha = vector<int>(4, 0);
									mha[0] = m1;
									mha[1] = m2;
									mha[2] = m3;
									mha[3] = m4;

									vector<int> throt = vector<int>(4, 0);
									throt[0] = throttling[s1];
									throt[1] = throttling[s2];
									throt[2] = throttling[s3];
									throt[3] = throttling[s4];

									double metval = processConfiguration(mha, throt, measurements);

									if(metval > bestMetricValue){
										bestMetricValue = metval;
										bestMHA = mha;
										bestThrot = throt;
									}
								}
							}
						}
					}
				}
			}
		}
	}
}

double
ModelThrottlingPolicy::processConfiguration(std::vector<int> mha,
											std::vector<int> throttle,
											PerformanceMeasurement* measurements){

	DPRINTF(MissBWPolicyExtra, "Processing configuration:\n");
	traceVerboseVector("MHA:        ", mha);
	traceVerboseVector("Throttling: ", throttle);

	// Step 1: Estimate number of inserted requests
	vector<int> insertedRequests = vector<int>(cpuCount, 0);
	for(int i=0;i<cpuCount;i++){
		insertedRequests[i] = estimateInsertedRequests(i, mha[i], throttle[i], measurements);
	}
	traceVerboseVector("Estimating request count to: ", insertedRequests);

	// Step 2: Estimate average memory latency with this request count

	// Step 3: Estimate performance with these memory latencies and compute metric value

	fatal("processConfiguration implementation not finished");
	return 0.0;
}

int
ModelThrottlingPolicy::estimateInsertedRequests(int cpuID, int mshrs, int throttling, PerformanceMeasurement* measurements){
	fatal("TODO: implement request estimation procedure");
}

#ifndef DOXYGEN_SHOULD_SKIP_THIS

BEGIN_DECLARE_SIM_OBJECT_PARAMS(ModelThrottlingPolicy)
	SimObjectParam<InterferenceManager* > interferenceManager;
	Param<Tick> period;
	Param<int> cpuCount;
	Param<string> performanceEstimationMethod;
	Param<bool> persistentAllocations;
	Param<int> iterationLatency;
	Param<string> optimizationMetric;
	Param<bool> enforcePolicy;
END_DECLARE_SIM_OBJECT_PARAMS(ModelThrottlingPolicy)

BEGIN_INIT_SIM_OBJECT_PARAMS(ModelThrottlingPolicy)
	INIT_PARAM_DFLT(interferenceManager, "The system's interference manager" , NULL),
	INIT_PARAM_DFLT(period, "The number of clock cycles between each decision", 1048576),
	INIT_PARAM(cpuCount, "The number of cpus in the system"),
	INIT_PARAM(performanceEstimationMethod, "The method to use for performance estimations"),
	INIT_PARAM_DFLT(persistentAllocations, "The method to use for performance estimations", true),
	INIT_PARAM_DFLT(iterationLatency, "The number of cycles it takes to evaluate one MHA", 0),
	INIT_PARAM_DFLT(optimizationMetric, "The metric to optimize for", "hmos"),
	INIT_PARAM_DFLT(enforcePolicy, "Should the policy be enforced?", true)
END_INIT_SIM_OBJECT_PARAMS(ModelThrottlingPolicy)

CREATE_SIM_OBJECT(ModelThrottlingPolicy)
{

	BasePolicy::PerformanceEstimationMethod perfEstMethod =
		BasePolicy::parsePerformanceMethod(performanceEstimationMethod);

	Metric* performanceMetric = BasePolicy::parseOptimizationMetric(optimizationMetric);

	return new ModelThrottlingPolicy(getInstanceName(),
							         interferenceManager,
							         period,
							         cpuCount,
							         perfEstMethod,
							         persistentAllocations,
							         iterationLatency,
							         performanceMetric,
							         enforcePolicy);
}

REGISTER_SIM_OBJECT("ModelThrottlingPolicy", ModelThrottlingPolicy)

#endif //DOXYGEN_SHOULD_SKIP_THIS


