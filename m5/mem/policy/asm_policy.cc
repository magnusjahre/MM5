
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
					 int _epoch,
					 bool _doLLCAlloc,
					 double _maximumSpeedup,
					 bool _manageMemoryBus)
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
	doLLCAlloc = _doLLCAlloc;
	maximumSpeedup = _maximumSpeedup;
	manageMemoryBus = _manageMemoryBus;

	if(_cpuCount > 1){
		epochEvent = new ASREpochEvent(this, epoch);
		epochEvent->schedule(epoch);

		epochMeasurements = vector<ASREpochMeasurements>(_cpuCount, ASREpochMeasurements(_cpuCount));
		for(int i=0;i<epochMeasurements.size();i++) epochMeasurements[i].highPriCPU = i;
	}

	avgLLCMissAdditionalCycles = vector<double>(_cpuCount, 0.0);
	CARshared = vector<double>(_cpuCount, 0.0);
	CARalone = vector<double>(_cpuCount, 0.0);
	epochCumProbDistrib = vector<double>(_cpuCount, 0.0);
	updateProbabilityDistribution(vector<double>(_cpuCount, 1.0 / (double) _cpuCount));

	srand(240000);

	prepareASMTraces(_cpuCount);

	allocationTrace = RequestTrace(_name, "AllocationTrace");
	vector<string> header = vector<string>();
	for(int i=0;i<_cpuCount;i++){
		stringstream curstr;
		curstr << "CPU" << i;
		header.push_back(curstr.str());
	}
	allocationTrace.initalizeTrace(header);

	bandwidthTrace = RequestTrace(_name, "BandwidthTrace");
	bandwidthTrace.initalizeTrace(header);
}

void
ASMPolicy::initCurveTracefiles(){
	vector<string> header;
	for(int i=0;i<maxWays;i++){
		stringstream head;
		head << (i+1);
		if(i == 0) head << " way";
		else head << " ways";
		header.push_back(head.str());
	}

	slowdownCurveTraces.resize(cpuCount, RequestTrace());

	for(int i=0;i<cpuCount;i++){
		stringstream filename;
		filename << "SlowdownCurveTrace" << i;
		slowdownCurveTraces[i] = RequestTrace(name(), filename.str().c_str());
		slowdownCurveTraces[i].initalizeTrace(header);
	}
}

void
ASMPolicy::updateProbabilityDistribution(vector<double> probabilities){
	DPRINTF(ASRPolicyProgress, "Updating probability distribution\n");
	assert(probabilities.size() == epochCumProbDistrib.size());
	epochCumProbDistrib[0] = probabilities[0];
	DPRINTF(ASRPolicyProgress, "Probability %f for CPU 0 gives cumulative probability %f\n", probabilities[0], epochCumProbDistrib[0]);
	for(int i=1;i<epochCumProbDistrib.size();i++){
		epochCumProbDistrib[i] = epochCumProbDistrib[i-1] + probabilities[i];
		DPRINTF(ASRPolicyProgress, "Probability %f for CPU %d gives cumulative probability %f\n", probabilities[i], i, epochCumProbDistrib[i]);
	}
}

void
ASMPolicy::changeHighPriProcess(){
	double highPriProb = rand() / (double) RAND_MAX;
	int newHighPriCPUID = -1;
	for(int i=0;i<epochCumProbDistrib.size();i++){
		if(highPriProb <= epochCumProbDistrib[i]){
			DPRINTF(ASRPolicy, "Random value %f is less than CPU%d end value %f\n", highPriProb, i, epochCumProbDistrib[i]);
			assert(newHighPriCPUID == -1);
			newHighPriCPUID = i;
			break;
		}
	}
	assert(newHighPriCPUID != -1);

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
	curAllocation = vector<int>(cpuCount, maxWays/cpuCount);
	initCurveTracefiles();

	if(cpuCount > 1){
		changeHighPriProcess();

		if(doLLCAlloc){
			for(int i=0;i<sharedCaches.size();i++){
				sharedCaches[i]->setCachePartition(curAllocation);
				sharedCaches[i]->enablePartitioning();
			}
		}
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

		if(maximumSpeedup != 0.0){
			if(values[i].speedup >= maximumSpeedup){
				DPRINTF(ASRPolicyProgress, "Speed-up %f greater than max %f, capping\n",
						values[i].speedup,
						maximumSpeedup);
				values[i].speedup = maximumSpeedup;
			}
			else{
				DPRINTF(ASRPolicyProgress, "Speed-up %f is less than cap %f, not capping\n", values[i].speedup, maximumSpeedup);
			}
		}
		else{
			DPRINTF(ASRPolicyProgress, "Capping disabled for CPU %d\n", i);
		}

		asrPrivateModeSpeedupEsts[i] = values[i].speedup;
		DPRINTF(ASRPolicyProgress, "CPU %d private to shared mode speed-up is %f with CAR-alone %f and CAR-shared %f\n",
				i,
				asrPrivateModeSpeedupEsts[i],
				values[i].carAlone,
				values[i].carShared);

		CARshared[i] = values[i].carShared;
		CARalone[i] = values[i].carAlone;
		avgLLCMissAdditionalCycles[i] = 0.0;
		if(values[i].avgMissTime > values[i].avgHitTime) avgLLCMissAdditionalCycles[i] = values[i].avgMissTime - values[i].avgHitTime;
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
ASMPolicy::setEpochProbabilities(std::vector<vector<double> > slowdowns, std::vector<int> llcQuotas){
	vector<double> estimatedSlowdowns = vector<double>(cpuCount, 0.0);

	double slowdownSum = 0.0;
	for(int i=0;i<llcQuotas.size();i++){
		estimatedSlowdowns[i] = slowdowns[i][llcQuotas[i]-1];
		DPRINTF(ASRPolicyProgress, "CPU %d: estimating slowdown %f from speed-up %f with allocation %d (index %d)\n",
				i,
				estimatedSlowdowns[i],
				slowdowns[i][llcQuotas[i]-1],
				llcQuotas[i],
				llcQuotas[i]-1);

		slowdownSum += estimatedSlowdowns[i];
	}

	vector<double> probabilities = vector<double>(cpuCount, 0.0);
	for(int i=0;i<probabilities.size();i++){
		probabilities[i] = estimatedSlowdowns[i] / slowdownSum;
		DPRINTF(ASRPolicyProgress, "CPU %d: next epoch probability is %f (%f / %f)\n",
				i,
				probabilities[i],
				estimatedSlowdowns[i],
				slowdownSum);
	}
	updateProbabilityDistribution(probabilities);

	vector<RequestTraceEntry> tracedata = vector<RequestTraceEntry>();
	for(int i=0;i<probabilities.size();i++) tracedata.push_back(probabilities[i]);
	bandwidthTrace.addTrace(tracedata);
}

void
ASMPolicy::runPolicy(PerformanceMeasurement measurements){

	if(!doLLCAlloc) return;
	DPRINTF(ASRPolicyProgress, "Running the ASR Cache Policy\n");

	vector<vector<double> > slowdowns(cpuCount, vector<double>(maxWays, 0.0));
	for(int i=0;i<slowdowns.size();i++){
		double curHits = measurements.perCoreCacheMeasurements[i].privateCumulativeCacheHits[curAllocation[i]-1];
		double llcAccesses = measurements.perCoreCacheMeasurements[i].accesses;

		DPRINTF(ASRPolicyProgress, "CPU %d: computing current hits %f, current LLC accesses %f, avg LLC additional miss cycles %f, CAR shared %f and CAR alone %f\n",
				i, curHits, llcAccesses, avgLLCMissAdditionalCycles[i], CARshared[i], CARalone[i]);

		for(int j=0;j<maxWays;j++){
			double hits = measurements.perCoreCacheMeasurements[i].privateCumulativeCacheHits[j];
			double deltaHits = hits - curHits;
			double CARn = llcAccesses / (period - (deltaHits * avgLLCMissAdditionalCycles[i]));

			if(CARn != 0.0) slowdowns[i][j] = CARalone[i] / CARn;
			if(slowdowns[i][j] < 1.0){
				DPRINTF(ASRPolicyProgress, "CPU %d: computed slowdown %f is less than 1.0, capping\n", i, slowdowns[i][j]);
				slowdowns[i][j] = 1.0;
			}
			assert(slowdowns[i][j] >= 1.0);

			DPRINTF(ASRPolicyProgress, "CPU %d: computed slowdown %f with hits %f, delta hits %f and CARn %f\n",
					i, slowdowns[i][j], hits, deltaHits, CARn);
		}

		std::vector<RequestTraceEntry> speedupTraceData = getTraceCurveDbl(slowdowns[i]);
		slowdownCurveTraces[i].addTrace(speedupTraceData);
	}

	assert(!sharedCaches.empty());
	curAllocation = sharedCaches[0]->lookaheadCachePartitioning(slowdowns, 0, false);

	vector<RequestTraceEntry> tracedata = vector<RequestTraceEntry>();
	for(int i=0;i<curAllocation.size();i++) tracedata.push_back(curAllocation[i]);
	allocationTrace.addTrace(tracedata);

	for(int i=0;i<sharedCaches.size();i++){
		sharedCaches[i]->setCachePartition(curAllocation);
		sharedCaches[i]->enablePartitioning();
	}

	if(manageMemoryBus){
		setEpochProbabilities(slowdowns, curAllocation);
	}
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
	Param<bool> allocateLLC;
	Param<double> maximumSpeedup;
	Param<bool> manageMemoryBus;
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
	INIT_PARAM(epoch, "The number of cycles in each epoch"),
	INIT_PARAM(allocateLLC, "Do LLC allocation?"),
	INIT_PARAM_DFLT(maximumSpeedup, "Cap speedup at this value", 0.0),
	INIT_PARAM(manageMemoryBus, "Do memory bus management?")
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
									  epoch,
									  allocateLLC,
									  maximumSpeedup,
									  manageMemoryBus);
}

REGISTER_SIM_OBJECT("ASMPolicy", ASMPolicy)

#endif //DOXYGEN_SHOULD_SKIP_THIS


