
#include <mem/policy/asm_policy.hh>
#include <cstdlib>

using namespace std;


ASMPolicy::ASMPolicy(std::string _name,
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

	prepareASMTraces(_cpuCount);
}

void
ASMPolicy::changeHighPriProcess(){
	int newHighPriCPUID = (rand() / (double) RAND_MAX) * cpuCount;
	DPRINTF(ASRPolicy, "Changing high priority process from %d to %d\n", curHighPriCPUID, newHighPriCPUID);
	curHighPriCPUID = newHighPriCPUID;

	intManager->setASRHighPriCPUID(newHighPriCPUID);
}

void
ASMPolicy::initPolicy(){
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
ASMPolicy::handleEpochEvent(){
	if(curTick % period != 0){
		DPRINTF(ASRPolicy, "===== Handling epoch event, high pri CPU %d, accumulating measurements\n", curHighPriCPUID);
		intManager->asrEpocMeasurements.finalizeEpoch(epoch);
		epochMeasurements[curHighPriCPUID].addValues(&(intManager->asrEpocMeasurements));

		DPRINTF(ASRPolicy, "Resetting epoch measurements\n");
		intManager->asrEpocMeasurements.epochReset();

		changeHighPriProcess();
	}
	else{
		DPRINTF(ASRPolicy, "Skipping epoch event handling due to policy event in the same cycle\n");
	}
}

void
ASMPolicy::prepareEstimates(){
	DPRINTF(ASRPolicyProgress, "===== Handling policy event, high pri CPU %d, accumulating measurements\n", curHighPriCPUID);
	intManager->asrEpocMeasurements.finalizeEpoch(epoch);
	epochMeasurements[curHighPriCPUID].addValues(&(intManager->asrEpocMeasurements));
	for(int i=0;i<cpuCount;i++){
		epochMeasurements[i].cpuATDHits = intManager->asrEpocMeasurements.cpuATDHits;
		epochMeasurements[i].cpuATDMisses = intManager->asrEpocMeasurements.cpuATDMisses;
		epochMeasurements[i].cpuSharedLLCAccesses = intManager->asrEpocMeasurements.cpuSharedLLCAccesses;
	}

	vector<ASMValues> values = vector<ASMValues>(cpuCount, ASMValues());

	for(int i=0;i<cpuCount;i++){
		epochMeasurements[i].computeCARAlone(i, epoch, &values.at(i));
		epochMeasurements[i].computeCARShared(i, period, &values.at(i));

		assert(values[i].carShared >= 0.0);
		if(values[i].carAlone == 0.0) values[i].speedup = 1.0;
		else values[i].speedup = values[i].carAlone / values[i].carShared;

		asrPrivateModeSpeedupEsts[i] = values[i].speedup;
		DPRINTF(ASRPolicyProgress, "CPU %d private to shared mode speed-up is %f with CAR-alone %f and CAR-shared %f\n",
				i,
				asrPrivateModeSpeedupEsts[i],
				values[i].carAlone,
				values[i].carShared);
	}

	int numEpochs = 0;
	for(int i=0;i<cpuCount;i++){
		values[i].numEpochs = epochMeasurements[i].epochCount;
		numEpochs += epochMeasurements[i].epochCount;
	}
	assert(numEpochs*epoch == period);

	traceASMValues(values);

	DPRINTF(ASRPolicyProgress, "Resetting quantum and epoch measurements\n");
	for(int i=0;i<cpuCount;i++) epochMeasurements[i].quantumReset();
	intManager->asrEpocMeasurements.quantumReset();
	changeHighPriProcess();
}

void
ASMPolicy::runPolicy(PerformanceMeasurement measurements){
	//TODO: Implement allocation policy
}

bool
ASMPolicy::doEvaluation(int cpuID){
	fatal("doEvaluation not implemented");
	return false;
}

void
ASMPolicy::prepareASMTraces(int numCPUs){
	vector<string> headers;
	headers.push_back("Shared LLC Accesses");
	headers.push_back("ATD Accesses");
	headers.push_back("Total Cycles");
	headers.push_back("Private Mode Hit Fraction");
	headers.push_back("Private Mode Hit Estimate");
	headers.push_back("Contention Misses");
	headers.push_back("Avg Miss Time");
	headers.push_back("Avg Hit Time");
	headers.push_back("Excess Cycles");
	headers.push_back("Private Mode Miss Fraction");
	headers.push_back("Private Mode Miss Estimate");
	headers.push_back("Avg Queuing Delay");
	headers.push_back("Queuing Delay");
	headers.push_back("CAR Alone (x10^6)");

	headers.push_back("Shared Mode LLC Accesses");
	headers.push_back("CAR Shared (x10^6)");

	headers.push_back("Speedup");
	headers.push_back("Number of Epochs");

	asmTraces.resize(cpuCount, RequestTrace());
	for(int i=0;i<cpuCount;i++){
		asmTraces[i] = RequestTrace(name(), RequestTrace::buildFilename("ASM", i).c_str());
		asmTraces[i].initalizeTrace(headers);
	}
}

void
ASMPolicy::traceASMValues(std::vector<ASMValues> values){
	for(int i=0;i<values.size();i++){
		vector<RequestTraceEntry> data;

		data.push_back(values[i].sharedLLCAccesses);
		data.push_back(values[i].atdAccesses);
		data.push_back(values[i].totalCycles);
		data.push_back(values[i].pmHitFraction);
		data.push_back(values[i].pmHitEstimate);
		data.push_back(values[i].contentionMisses);
		data.push_back(values[i].avgMissTime);
		data.push_back(values[i].avgHitTime);
		data.push_back(values[i].excessCycles);
		data.push_back(values[i].pmMissFraction);
		data.push_back(values[i].pmMissEstimate);
		data.push_back(values[i].avgQueuingDelay);
		data.push_back(values[i].queuingDelay);
		data.push_back(values[i].carAlone*1000000);

		data.push_back(values[i].cpuSharedLLCAccesses);
		data.push_back(values[i].carShared*1000000);

		data.push_back(values[i].speedup);
		data.push_back(values[i].numEpochs);

		asmTraces[i].addTrace(data);
	}
}

#ifndef DOXYGEN_SHOULD_SKIP_THIS

BEGIN_DECLARE_SIM_OBJECT_PARAMS(ASMPolicy)
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
END_DECLARE_SIM_OBJECT_PARAMS(ASMPolicy)

BEGIN_INIT_SIM_OBJECT_PARAMS(ASMPolicy)
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
END_INIT_SIM_OBJECT_PARAMS(ASMPolicy)

CREATE_SIM_OBJECT(ASMPolicy)
{
	BasePolicy::PerformanceEstimationMethod perfEstMethod =
		BasePolicy::parsePerformanceMethod(performanceEstimationMethod);

	Metric* performanceMetric = BasePolicy::parseOptimizationMetric(optimizationMetric);

	BasePolicy::WriteStallTechnique wst = BasePolicy::parseWriteStallTech(writeStallTechnique);
	BasePolicy::PrivBlockedStallTechnique pbst = BasePolicy::parsePrivBlockedStallTech(privateBlockedStallTechnique);

	BasePolicy::EmptyROBStallTechnique rst = BasePolicy::parseEmptyROBStallTech(emptyROBStallTechnique);

	return new ASMPolicy(getInstanceName(),
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

REGISTER_SIM_OBJECT("ASMPolicy", ASMPolicy)

#endif //DOXYGEN_SHOULD_SKIP_THIS


