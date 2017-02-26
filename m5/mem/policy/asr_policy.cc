
#include "asr_policy.hh"
#include <cstdlib>

using namespace std;


ASRPolicy::ASRPolicy(std::string _name,
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
					 int _epoch)
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

	epoch = _epoch;
	if(_period < epoch) fatal("ASR quantum (aka period) has to be larger than the epoch");

	maxWays = 0;
	curHighPriCPUID = 0;

	if(_cpuCount > 1){
		epochEvent = new ASREpochEvent(this, epoch);
		epochEvent->schedule(epoch);

		epochMeasurements = vector<ASREpochMeasurements>(_cpuCount, ASREpochMeasurements(_cpuCount));
		for(int i=0;i<epochMeasurements.size();i++) epochMeasurements[i].highPriCPU = i;
	}

	srand(240000);
}

void
ASRPolicy::changeHighPriProcess(){
	int newHighPriCPUID = (rand() / (double) RAND_MAX) * cpuCount;
	DPRINTF(ASRPolicy, "Changing high priority process from %d to %d\n", curHighPriCPUID, newHighPriCPUID);
	curHighPriCPUID = newHighPriCPUID;

	intManager->setASRHighPriCPUID(newHighPriCPUID);
}

void
ASRPolicy::initPolicy(){
	if(cpuCount != 1){
		disableCommitSampling();
	}

	assert(!sharedCaches.empty());
	maxWays = sharedCaches[0]->getAssoc();

	if(cpuCount > 1){
		changeHighPriProcess();
	}
}

void
ASRPolicy::handleEpochEvent(){
	DPRINTF(ASRPolicy, "===== Handling epoch event, high pri CPU %d, accumulating measurements\n", curHighPriCPUID);
	epochMeasurements[curHighPriCPUID].addValueVector(intManager->asrEpocMeasurements.data);

	DPRINTF(ASRPolicy, "Resetting epoch measurements\n");
	intManager->asrEpocMeasurements.epochReset();

	changeHighPriProcess();
}

void
ASRPolicy::runPolicy(PerformanceMeasurement measurements){
	intManager->asrEpocMeasurements.quantumReset();
	fatal("runPolicy not implemented");
}

bool
ASRPolicy::doEvaluation(int cpuID){
	fatal("doEvaluation not implemented");
	return false;
}

#ifndef DOXYGEN_SHOULD_SKIP_THIS

BEGIN_DECLARE_SIM_OBJECT_PARAMS(ASRPolicy)
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
	Param<int> epoch;
END_DECLARE_SIM_OBJECT_PARAMS(ASRPolicy)

BEGIN_INIT_SIM_OBJECT_PARAMS(ASRPolicy)
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
	INIT_PARAM(epoch, "The number of cycles in each epoch")
END_INIT_SIM_OBJECT_PARAMS(ASRPolicy)

CREATE_SIM_OBJECT(ASRPolicy)
{
	BasePolicy::PerformanceEstimationMethod perfEstMethod =
		BasePolicy::parsePerformanceMethod(performanceEstimationMethod);

	Metric* performanceMetric = BasePolicy::parseOptimizationMetric(optimizationMetric);

	BasePolicy::WriteStallTechnique wst = BasePolicy::parseWriteStallTech(writeStallTechnique);
	BasePolicy::PrivBlockedStallTechnique pbst = BasePolicy::parsePrivBlockedStallTech(privateBlockedStallTechnique);

	BasePolicy::EmptyROBStallTechnique rst = BasePolicy::parseEmptyROBStallTech(emptyROBStallTechnique);

	return new ASRPolicy(getInstanceName(),
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
									  epoch);
}

REGISTER_SIM_OBJECT("ASRPolicy", ASRPolicy)

#endif //DOXYGEN_SHOULD_SKIP_THIS


