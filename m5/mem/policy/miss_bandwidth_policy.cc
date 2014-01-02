/*
 * miss_bandwidth_policy.cc
 *
 *  Created on: May 8, 2010
 *      Author: jahre
 */

#include "miss_bandwidth_policy.hh"

MissBandwidthPolicy::MissBandwidthPolicy(std::string _name,
										 InterferenceManager* _intManager,
										 Tick _period,
										 int _cpuCount,
										 PerformanceEstimationMethod _perfEstMethod,
										 bool _persistentAllocations,
										 int _iterationLatency,
										 Metric* _performanceMetric,
										 bool _enforcePolicy,
					   	    			 ThrottleControl* _sharedCacheThrottle,
					   	    			 std::vector<ThrottleControl* > _privateCacheThrottles,
										 double _busUtilThreshold,
										 double _cutoffReqInt,
										 RequestEstimationMethod _reqEstMethod,
										 double _acceptanceThreshold,
										 double _reqVariationThreshold,
										 int _renewMeasurementsThreshold,
										 SearchAlgorithm _searchAlgorithm,
										 double _busRequestThresholdIntensity,
										 WriteStallTechnique _wst,
										 PrivBlockedStallTechnique _pbst,
										 EmptyROBStallTechnique _rst,
										 double _cplCutoff,
										 double _latencyCutoff)
: BasePolicy(_name,
		_intManager,
		_period,
		_cpuCount,
		_perfEstMethod,
		_persistentAllocations,
		_iterationLatency,
		_performanceMetric,
		_enforcePolicy,
		_sharedCacheThrottle,
		_privateCacheThrottles,
		_wst,
		_pbst,
		_rst,
		_cplCutoff,
		_latencyCutoff)
{

	level = 0;
	maxMetricValue = 0;

	if(_cutoffReqInt < 1){
		fatal("The request threshold unit is number of requests, a value less than 1 does not make sense");
	}

	if(_busRequestThresholdIntensity < 1){
		fatal("The bus request threshold unit is number of requests, a value less than 1 does not make sense");
	}

	reqEstMethod = _reqEstMethod;
	busUtilizationThreshold = _busUtilThreshold;
	requestCountThreshold = _cutoffReqInt;
	busRequestThreshold = _busRequestThresholdIntensity;
	acceptanceThreshold = _acceptanceThreshold;
	requestVariationThreshold = _reqVariationThreshold;
	renewMeasurementsThreshold = _renewMeasurementsThreshold;
	searchAlgorithm = _searchAlgorithm;

	renewMeasurementsCounter = 0;
}

void
MissBandwidthPolicy::runPolicy(PerformanceMeasurement measurements){
	addTraceEntry(&measurements);

	assert(currentMeasurements == NULL);
	currentMeasurements = &measurements;

	DPRINTF(MissBWPolicy, "--- Running Miss Bandwidth Policy\n");

	for(int i=0;i<requestAccumulator.size();i++){
		requestAccumulator[i] += currentMeasurements->requestsInSample[i];

		double tmpCurMes = (double) currentMeasurements->requestsInSample[i];
		requestSqAccumulator[i] += tmpCurMes*tmpCurMes;
	}
	traceVector("Request accumulator: ", requestAccumulator);
	traceVector("Request square accumulator: ", requestSqAccumulator);
	for(int i=0;i<cpuCount;i++){
		DPRINTFR(MissBWPolicy, "CPU %d, %d cache misses, %d interference misses, %d accesses\n",
				i,
				currentMeasurements->perCoreCacheMeasurements[i].readMisses,
				currentMeasurements->perCoreCacheMeasurements[i].interferenceMisses,
				currentMeasurements->perCoreCacheMeasurements[i].accesses);
	}


	renewMeasurementsCounter++;
	if(usePersistentAllocations && renewMeasurementsCounter < renewMeasurementsThreshold && renewMeasurementsCounter > 1){
		DPRINTF(MissBWPolicy, "Skipping Miss Bandwidth Policy due to Persistent Allocations, counter is %d, threshold %d\n",
							  renewMeasurementsCounter,
							  renewMeasurementsThreshold);
		traceNumMSHRs();

		if(renewMeasurementsCounter == 2){
			assert(!bestRequestProjection.empty());
			traceBestProjection();

			DPRINTF(MissBWPolicy, "Dumping accuracy information\n");
			traceVector("Request Count Measurement: ", currentMeasurements->requestsInSample);
			traceVector("Shared Latency Measurement: ", currentMeasurements->sharedLatencies);
		}

		currentMeasurements = NULL;
		return;
	}

	traceVector("Request Count Measurement: ", currentMeasurements->requestsInSample);
	traceVector("Shared Latency Measurement: ", currentMeasurements->sharedLatencies);
	traceVector("Committed Instructions: ", currentMeasurements->committedInstructions);
	traceVector("CPU Stall Cycles: ", currentMeasurements->cpuStallCycles);
	vector<double> actualIPC(cpuCount, 0.0);
	for(int i=0;i<cpuCount;i++) actualIPC[i] = (double) currentMeasurements->committedInstructions[i] / (double) period;
	traceVector("Measured IPC: ", actualIPC);

	if(measurementsValid){
		for(int i=0;i<cpuCount;i++){
			if(currentMeasurements->requestsInSample[i] > requestCountThreshold){
				double reqError = computeError(bestRequestProjection[i], measurements.requestsInSample[i]);
				requestRelError[i].sample(reqError);
				requestAbsError[i].sample(bestRequestProjection[i] - measurements.requestsInSample[i]);

				if(currentMeasurements->sharedLatencies[i] > 0){
					double latError = computeError(bestLatencyProjection[i], measurements.sharedLatencies[i]);
					sharedLatencyRelError[i].sample(latError);
					sharedLatencyAbsError[i].sample(bestLatencyProjection[i] - measurements.sharedLatencies[i]);
				}
			}
			else{
				DPRINTF(MissBWPolicy, "Skipping accuracy check for CPU %i, %d requests < threshold %f\n", i, currentMeasurements->requestsInSample[i], requestCountThreshold);
			}
		}
	}


	updateMWSEstimates();

	updateAloneIPCEstimate();
	traceVector("Alone IPC Estimates: ", aloneIPCEstimates);

	if(renewMeasurementsCounter >= renewMeasurementsThreshold){
		DPRINTF(MissBWPolicy, "Renew counter (%d) >= threshold (%d), increasing all MHAs to maximum (%d)\n",
				              renewMeasurementsCounter,
				              renewMeasurementsThreshold,
				              maxMSHRs);

		renewMeasurementsCounter = 0;

		for(int i=0;i<caches.size();i++) caches[i]->setNumMSHRs(maxMSHRs);

		computeRequestStatistics();
	}
	else{

		if(measurementsValid && !usePersistentAllocations){
			traceBestProjection();
		}

		for(int i=0;i<requestAccumulator.size();i++){
			requestAccumulator[i] = 0;
			requestSqAccumulator[i] = 0;
		}
		traceVector("Request accumulator is now: ", requestAccumulator);
		traceVector("Request square accumulator is now: ", requestSqAccumulator);


		// initalize best-storage
		for(int i=0;i<bestRequestProjection.size();i++) bestRequestProjection[i] = (double) currentMeasurements->requestsInSample[i];
		bestLatencyProjection = currentMeasurements->sharedLatencies;

		vector<int> bestMHA;
		Tick desicionLatency = 0;
		if(searchAlgorithm == EXHAUSTIVE_SEARCH){
			double tmpLat = pow((double) maxMSHRs, (double) cpuCount);
			int tmpIntLat = (int) tmpLat;
			desicionLatency = tmpIntLat * iterationLatency;
			bestMHA = exhaustiveSearch();
		}
		else if(searchAlgorithm == BUS_SORTED){
			desicionLatency = maxMSHRs * cpuCount * iterationLatency;
			bestMHA = busSearch(false);
		}
		else if(searchAlgorithm == BUS_SORTED_LOG){
			desicionLatency = (FloorLog2(maxMSHRs)+1) * cpuCount * iterationLatency;
			bestMHA = busSearch(true);
		}
		else{
			fatal("Unknown search algorithm");
		}

		measurementsValid = true;
		if(bestMHA.size() != cpuCount){
			DPRINTF(MissBWPolicy, "All programs have to few requests, reverting to max MSHRs configuration\n");
			bestMHA.resize(cpuCount, maxMSHRs);
		}
		traceVector("Best MHA: ", bestMHA);
		DPRINTF(MissBWPolicy, "Best metric value is %f\n", maxMetricValue);

		double currentMetricValue = computeCurrentMetricValue();
		double benefit = maxMetricValue / currentMetricValue;
		if(benefit > acceptanceThreshold){
			DPRINTF(MissBWPolicy, "Scheduling implementation of new MHA, benefit is %d, acceptance threshold %d, latency %d\n", benefit, acceptanceThreshold, desicionLatency);

			MissBandwidthImplementMHAEvent* implEvent = new MissBandwidthImplementMHAEvent(this, bestMHA);
			implEvent->schedule(curTick + desicionLatency);

		}
		else{
			DPRINTF(MissBWPolicy, "Benefit from new best MHA is too low (%f < %f), new MHA not chosen\n", benefit, acceptanceThreshold);
		}

		traceAloneIPC(measurements.requestsInSample,
				      aloneIPCEstimates,
				      measurements.committedInstructions,
				      measurements.cpuStallCycles,
				      measurements.sharedLatencies,
				      measurements.avgMissesWhileStalled);
	}

	traceNumMSHRs();


	currentMeasurements = NULL;
}

std::vector<int>
MissBandwidthPolicy::exhaustiveSearch(){

	vector<int> value(cpuCount, 0);
	level = -1;
	maxMetricValue = 0.0;

	recursiveExhaustiveSearch(&value, 0);

	return relocateMHA(&maxMHAConfiguration);
}


void
MissBandwidthPolicy::recursiveExhaustiveSearch(std::vector<int>* value, int k){

	assert(value->size() == cpuCount);

	level = level+1;

	if(level >= 1){
		assert(level-1 >= 0 && level-1 < value->size());
		(*value)[level-1] = k;
	}

	if(level == cpuCount){
		double metricValue = evaluateMHA(relocateMHA(value));
		if(metricValue >= maxMetricValue){

			if(metricValue > 0) DPRINTFR(MissBWPolicyExtra, "Metric value %f is larger than previous best %f, new best MHA\n", metricValue, maxMetricValue);

			maxMetricValue = metricValue;
			maxMHAConfiguration = *value;

			updateBestProjections();
		}
	}
	else{
		for(int i=0;i<maxMSHRs;i++){
			recursiveExhaustiveSearch(value, i);
		}
	}

	level = level - 1;
}

std::vector<int>
MissBandwidthPolicy::busSearch(bool onlyPowerOfTwoMSHRs){

	traceVector("Bus accesses per core: ", currentMeasurements->busAccessesPerCore);

	vector<int> processorIDOrder;
	vector<bool> marked(cpuCount, false);
	for(int i=0;i<cpuCount;i++){
		int minval = 0;
		int minIndex = -1;
		for(int j=0;j<cpuCount;j++){
			int tmpReqs = currentMeasurements->busAccessesPerCore[j];
			if(tmpReqs >= minval && !marked[j]){
				minval = tmpReqs;
				minIndex = j;
			}
		}
		assert(minIndex != -1);

		processorIDOrder.push_back(minIndex);
		marked[minIndex] = true;
	}

	traceVector("Search order: ", processorIDOrder);
	vector<RequestTraceEntry> searchTraceData;
	for(int i=0;i<processorIDOrder.size();i++) searchTraceData.push_back(processorIDOrder[i]);

	vector<int> bestMHA(cpuCount, maxMSHRs);
	maxMetricValue = 0.0;

	for(int i=0;i<processorIDOrder.size();i++){
		int currentTestMSHRs = 1;
		while(currentTestMSHRs <= maxMSHRs){
			vector<int> testMHA = bestMHA;
			testMHA[processorIDOrder[i]] = currentTestMSHRs;

			traceVector("Evaluating MHA: ", testMHA);
			double curMetricValue = evaluateMHA(testMHA);
			searchTraceData.push_back(curMetricValue);
			if(curMetricValue >= maxMetricValue){
				DPRINTF(MissBWPolicy, "New best config (%f > %f)\n", curMetricValue, maxMetricValue);
				bestMHA[processorIDOrder[i]] = currentTestMSHRs;
				maxMetricValue = curMetricValue;

				updateBestProjections();
			}

			if(onlyPowerOfTwoMSHRs) currentTestMSHRs = currentTestMSHRs << 1;
			else currentTestMSHRs++;
		}
	}

	searchTrace.addTrace(searchTraceData);

	return bestMHA;
}

vector<int>
MissBandwidthPolicy::relocateMHA(std::vector<int>* mhaConfig){

	vector<int> configCopy(*mhaConfig);

	for(int i=0;i<configCopy.size();i++){
		configCopy[i]++;
	}

	return configCopy;
}


double
MissBandwidthPolicy::evaluateMHA(std::vector<int> currentMHA){

	// 1. Prune search space
	for(int i=0;i<currentMHA.size();i++){
		if(currentMHA[i] < maxMSHRs){
			if(!doEvaluation(i)){
				traceVerboseVector("Pruning MHA: ", currentMHA);
				return 0.0;
			}
		}
	}

	traceVerboseVector("--- Evaluating MHA: ", currentMHA);

	traceVerboseVector("Request Count Measurement: ", currentMeasurements->requestsInSample);
	traceVerboseVector("Shared Latency Measurement: ", currentMeasurements->sharedLatencies);
	traceVerboseVector("Bus Request Measurement: ", currentMeasurements->busAccessesPerCore);
	traceVerboseVector("Bus Read Measurement: ", currentMeasurements->busReadsPerCore);
	for(int i=0;i<cpuCount;i++){
		DPRINTFR(MissBWPolicyExtra, "CPU %d, %d cache misses, %d interference misses, %d accesses\n",
				i,
				currentMeasurements->perCoreCacheMeasurements[i].readMisses,
				currentMeasurements->perCoreCacheMeasurements[i].interferenceMisses,
				currentMeasurements->perCoreCacheMeasurements[i].accesses);
	}

	// 2. Estimate new shared memory latencies
	vector<double> sharedLatencyEstimates(cpuCount, 0.0);
	getAverageMemoryLatency(&currentMHA, &sharedLatencyEstimates);
	currentLatencyProjection = sharedLatencyEstimates;

	traceVerboseVector("Shared Latency Projection: ", sharedLatencyEstimates);

	// 3. Compute speedups
	vector<double> speedups(cpuCount, 0.0);
	vector<double> sharedIPCEstimates(cpuCount, 0.0);

	for(int i=0;i<cpuCount;i++){

		double newStallEstimate = 0.0;
		fatal("estimateStallCycles call must be fixed");

//		double privateMisses = currentMeasurements->perCoreCacheMeasurements[i].readMisses - currentMeasurements->perCoreCacheMeasurements[i].interferenceMisses;
//		double newStallEstimate = estimateStallCycles(currentMeasurements->cpuStallCycles[i],
//				                                      mostRecentMWSEstimate[i][caches[i]->getCurrentMSHRCount(true)],
//				                                      mostRecentMLPEstimate[i][caches[i]->getCurrentMSHRCount(true)],
//													  currentMeasurements->sharedLatencies[i],
//													  currentMeasurements->requestsInSample[i],
//													  mostRecentMWSEstimate[i][currentMHA[i]],
//													  mostRecentMLPEstimate[i][currentMHA[i]],
//													  sharedLatencyEstimates[i],
//													  currentMeasurements->requestsInSample[i],
//													  currentMeasurements->responsesWhileStalled[i],
//													  i,
//													  currentMeasurements->perCoreCacheMeasurements[i].readMisses,
//													  privateMisses);

		sharedIPCEstimates[i]= (double) currentMeasurements->committedInstructions[i] / (currentMeasurements->getNonStallCycles(i, period) + newStallEstimate);
		speedups[i] = computeSpeedup(sharedIPCEstimates[i], i);

		DPRINTFR(MissBWPolicyExtra, "CPU %d, current stall %d, estimating new stall time %f, new mws %f, new mlp %f, current stalled %d\n",
									i,
									currentMeasurements->cpuStallCycles[i],
									newStallEstimate,
									mostRecentMWSEstimate[i][currentMHA[i]],
									mostRecentMLPEstimate[i][currentMHA[i]],
									currentMeasurements->cpuStallCycles[i]);
	}

	currentIPCProjection = sharedIPCEstimates;
	tracePerformance(sharedIPCEstimates, speedups);

	// 4. Compute metric
	double metricValue =  performanceMetric->computeMetric(&speedups, &sharedIPCEstimates);

//	if(dumpSearchSpaceAt == curTick){
//		dumpSearchSpace(mhaConfig, metricValue);
//	}

	DPRINTFR(MissBWPolicyExtra, "Returning metric value %f\n", metricValue);

	return metricValue;
}

bool
MissBandwidthPolicy::doEvaluation(int cpuID){

	DPRINTFR(MissBWPolicyExtra, "Checking if cpu %d is should be searched with threshold %f\n", cpuID, requestCountThreshold);
	if(currentMeasurements->requestsInSample[cpuID] < requestCountThreshold){
		DPRINTFR(MissBWPolicyExtra, "Skipping, %d requests are less than threshold %d\n", currentMeasurements->requestsInSample[cpuID], requestCountThreshold);
		return false;
	}

	double relativeVariation = reqsPerSampleStdDev[cpuID] / avgReqsPerSample[cpuID];
	if(relativeVariation > requestVariationThreshold){
		DPRINTFR(MissBWPolicyExtra, "Skipping, relative variation %f is larger than threshold %f\n", relativeVariation, requestVariationThreshold);
		return false;
	}

	return true;
}

double
MissBandwidthPolicy::computeRequestScalingRatio(int cpuID, int newMSHRCount){

	int currentMSHRCount = caches[cpuID]->getCurrentMSHRCount(true);

	if(reqEstMethod == MWS){

		if(mostRecentMWSEstimate[cpuID][currentMSHRCount] == 0){
			return 1;
		}
		else{
			return mostRecentMWSEstimate[cpuID][newMSHRCount] / mostRecentMWSEstimate[cpuID][currentMSHRCount];
		}

	}
	else if(reqEstMethod == MLP){

		if(currentMeasurements->mlpEstimate[cpuID][newMSHRCount] == 0){
			return 1;
		}
		else{
			return currentMeasurements->mlpEstimate[cpuID][currentMSHRCount] / currentMeasurements->mlpEstimate[cpuID][newMSHRCount];
		}
	}
	fatal("unknown request scaling ratio");
	return 0.0;
}


void
MissBandwidthPolicy::getAverageMemoryLatency(vector<int>* currentMHA,
											 vector<double>* estimatedSharedLatencies){

	assert(estimatedSharedLatencies->size() == cpuCount);
	assert(currentMeasurements != NULL);

	vector<double> newRequestCountEstimates(cpuCount, 0.0);

	// 1. Estimate change in request count due change in MLP
	double freeBusSlots = 0;
	for(int i=0;i<cpuCount;i++){

		double mlpRatio = computeRequestScalingRatio(i, currentMHA->at(i));

		newRequestCountEstimates[i] = (double) currentMeasurements->requestsInSample[i] * mlpRatio;

		double tmpFreeBusSlots = currentMeasurements->busAccessesPerCore[i] * (1 - mlpRatio);
		freeBusSlots += tmpFreeBusSlots;

		DPRINTFR(MissBWPolicyExtra, "Request change for CPU %i, MLP ratio %f, request estimate %f, freed bus slots %f\n", i, mlpRatio, newRequestCountEstimates[i], tmpFreeBusSlots);
	}

	// 	1.5 Estimate the reduction in cache interference
	double newReqSum = 0.0;
	double oldReqSum = 0.0;
	for(int i=0;i<cpuCount;i++){
		newReqSum += newRequestCountEstimates[i];
		oldReqSum += (double) currentMeasurements->requestsInSample[i];
		if(currentMHA->at(i) == maxMSHRs) assert((int) newRequestCountEstimates[i] == currentMeasurements->requestsInSample[i]);
	}

	double accessFrequencyReduction = 1.0;
	if(oldReqSum != 0.0 && newReqSum != 0.0){
		accessFrequencyReduction = newReqSum / oldReqSum;
		DPRINTFR(MissBWPolicyExtra, "Computing access frequency reduction to %f, request sum with reduction is %f, old sum was %f\n",
				accessFrequencyReduction,
				newReqSum,
				oldReqSum);
	}
	assert(accessFrequencyReduction >= 0.0 && accessFrequencyReduction <= 1.0);

	vector<double> cacheHitLatencyReduction(cpuCount, 0.0);
	vector<double> interferenceMissProjection(cpuCount, 0.0);
	for(int i=0;i<cpuCount;i++){
		if(currentMHA->at(i) == maxMSHRs && currentMeasurements->requestsInSample[i] > 0){
			double numIntMisses = (double) currentMeasurements->perCoreCacheMeasurements[i].interferenceMisses;
			interferenceMissProjection[i] = numIntMisses * accessFrequencyReduction;

			double sumCacheIntLatency = currentMeasurements->cacheInterferenceLatency[i];
			cacheHitLatencyReduction[i] = (sumCacheIntLatency * (1 - accessFrequencyReduction)) / (double) currentMeasurements->requestsInSample[i];

			DPRINTFR(MissBWPolicyExtra, "Cache analysis for CPU %i, frequency reduction %f, sum cache interference %f, expected latency reduction is %f\n",
					i,
					accessFrequencyReduction,
					sumCacheIntLatency,
					cacheHitLatencyReduction[i]);
		}
		else{
			interferenceMissProjection[i] = (double) currentMeasurements->perCoreCacheMeasurements[i].interferenceMisses;
		}

	}
	currentInterferenceMissProjection = interferenceMissProjection;

	// 2. Estimate the response in request demand
	vector<double> additionalBusRequests(cpuCount, 0.0);
	for(int i=0;i<cpuCount;i++){

		if(currentMeasurements->busReadsPerCore[i] > busRequestThreshold
		   && currentMHA->at(i) == maxMSHRs){

			double queueRatio = 1.0;
			if(currentMeasurements->privateLatencyBreakdown[i][InterferenceManager::MemoryBusQueue] > 0){
				queueRatio = currentMeasurements->latencyBreakdown[i][InterferenceManager::MemoryBusQueue] / currentMeasurements->privateLatencyBreakdown[i][InterferenceManager::MemoryBusQueue];
			}

			additionalBusRequests[i] = ((double) currentMeasurements->busReadsPerCore[i]) * queueRatio;
			DPRINTFR(MissBWPolicyExtra, "CPU %d has queue ratio %f, estimating %f additional bus reqs\n", i, queueRatio, additionalBusRequests[i]);
		}
		else{
			DPRINTFR(MissBWPolicyExtra, "CPU %d has too few requests (%i < %i) or num MSHRs is not increased (new count %d < old count %d)\n",
					i,
					currentMeasurements->busReadsPerCore[i],
					busRequestThreshold,
					currentMHA->at(i),
					caches[i]->getCurrentMSHRCount(true));
		}
	}

	if(currentMeasurements->actualBusUtilization > busUtilizationThreshold && freeBusSlots > 0){
		DPRINTFR(MissBWPolicyExtra, "Bus utilzation is larger than threshold (%f > %f)\n", currentMeasurements->actualBusUtilization, busUtilizationThreshold);
		double addBusReqSum = computeSum(&additionalBusRequests);
		DPRINTFR(MissBWPolicyExtra, "Sum of additional requests of cpus without MSHR reduction is %d\n", addBusReqSum);

		if(addBusReqSum > freeBusSlots){
			for(int i=0;i<cpuCount;i++){
				double additionalReads = freeBusSlots * ((double) additionalBusRequests[i]) / ((double) addBusReqSum);
				newRequestCountEstimates[i] += additionalReads;

				DPRINTFR(MissBWPolicyExtra, "Estimating %f additional reads for CPU %d\n",
						additionalReads,
						i);

			}
		}
		else{
			for(int i=0;i<cpuCount;i++){
				newRequestCountEstimates[i] += additionalBusRequests[i];
			}
		}
	}
	else{
		DPRINTFR(MissBWPolicyExtra, "Bus utilzation smaller than threshold (%f < %f) or no free bus slots (%f)\n", currentMeasurements->actualBusUtilization, busUtilizationThreshold, freeBusSlots);
	}

	// 3. Estimate the new average request latency
	for(int i=0;i<cpuCount;i++){
		double currentAvgBusLat = currentMeasurements->latencyBreakdown[i][InterferenceManager::MemoryBusQueue];
		double newRequestRatio = 1.0;
        if(currentMeasurements->requestsInSample[i] > 0 && newRequestCountEstimates[i] > 0){
            newRequestRatio = ((double) currentMeasurements->requestsInSample[i]) / newRequestCountEstimates[i];
        }

        DPRINTFR(MissBWPolicyExtra, "CPU %d: New request ratio is %f, estimated new request count %f, old request count %d, miss rate %f\n",
									i,
									newRequestRatio,
									newRequestCountEstimates[i],
									currentMeasurements->requestsInSample[i],
									currentMeasurements->perCoreCacheMeasurements[i].getMissRate());

        double newAvgBusLat = newRequestRatio * currentAvgBusLat;

		(*estimatedSharedLatencies)[i] = (currentMeasurements->sharedLatencies[i] - currentAvgBusLat) + newAvgBusLat - cacheHitLatencyReduction[i];
		DPRINTFR(MissBWPolicyExtra, "CPU %i, estimating bus lat to %f, new bus lat %f, new avg lat %f, cache hit latency reduction %f\n",
				                    i,
				                    currentAvgBusLat,
				                    newAvgBusLat,
				                    estimatedSharedLatencies->at(i),
				                    cacheHitLatencyReduction[i]);

        assert(estimatedSharedLatencies->at(i) >= 0);
	}

	traceVerboseVector("Request Count Projection: ", newRequestCountEstimates);
	currentRequestProjection = newRequestCountEstimates;
}

void
MissBandwidthPolicy::computeRequestStatistics(){
	for(int i=0;i<requestAccumulator.size();i++){
		double mean = (double) requestAccumulator[i] / (double) (renewMeasurementsThreshold-1);
		avgReqsPerSample[i] = mean;

		double sqmean = (double) requestSqAccumulator[i] / (double) (renewMeasurementsThreshold-1);

		reqsPerSampleStdDev[i] = squareRoot(sqmean - mean*mean);
	}

	traceVector("Average Reqs per Sample: ", avgReqsPerSample);
	traceVector("Reqs per Sample standard deviation: ", reqsPerSampleStdDev);
}

template <class T>
T
MissBandwidthPolicy::computeSum(vector<T>* values){
	T sum = 0;
	for(int i=0;i<cpuCount;i++){
		sum += values->at(i);
	}
	return sum;
}

template <class T>
vector<double>
MissBandwidthPolicy::computePercetages(vector<T>* values){
	double sum = computeSum(values);

	vector<double> percentages(values->size(), 0.0);
	for(int i=0;i<cpuCount;i++){
		percentages[i] = values->at(i) / sum;
	}

	return percentages;
}


#ifndef DOXYGEN_SHOULD_SKIP_THIS

BEGIN_DECLARE_SIM_OBJECT_PARAMS(MissBandwidthPolicy)
	SimObjectParam<InterferenceManager* > interferenceManager;
	Param<Tick> period;
	Param<int> cpuCount;
	Param<string> performanceEstimationMethod;
	Param<bool> persistentAllocations;
	Param<int> iterationLatency;
	Param<string> optimizationMetric;
	Param<bool> enforcePolicy;
	SimObjectParam<ThrottleControl* > sharedCacheThrottle;
	SimObjectVectorParam<ThrottleControl* > privateCacheThrottles;
	Param<double> busUtilizationThreshold;
	Param<double> requestCountThreshold;
	Param<string> requestEstimationMethod;
	Param<double> acceptanceThreshold;
	Param<double> requestVariationThreshold;
	Param<double> renewMeasurementsThreshold;
	Param<string> searchAlgorithm;
	Param<double> busRequestThreshold;
	Param<string> writeStallTechnique;
	Param<string> privateBlockedStallTechnique;
	Param<string> emptyROBStallTechnique;
	Param<int> cplCutoff;
	Param<int> latencyCutoff;
END_DECLARE_SIM_OBJECT_PARAMS(MissBandwidthPolicy)

BEGIN_INIT_SIM_OBJECT_PARAMS(MissBandwidthPolicy)
	INIT_PARAM_DFLT(interferenceManager, "The system's interference manager" , NULL),
	INIT_PARAM_DFLT(period, "The number of clock cycles between each decision", 1048576),
	INIT_PARAM(cpuCount, "The number of cpus in the system"),
	INIT_PARAM(performanceEstimationMethod, "The method to use for performance estimations"),
	INIT_PARAM_DFLT(persistentAllocations, "The method to use for performance estimations", true),
	INIT_PARAM_DFLT(iterationLatency, "The number of cycles it takes to evaluate one MHA", 0),
	INIT_PARAM_DFLT(optimizationMetric, "The metric to optimize for", "hmos"),
	INIT_PARAM_DFLT(enforcePolicy, "Should the policy be enforced?", true),
	INIT_PARAM(sharedCacheThrottle, "Shared cache throttle"),
	INIT_PARAM(privateCacheThrottles, "Private cache throttles"),
	INIT_PARAM_DFLT(busUtilizationThreshold, "The actual bus utilzation to consider the bus as full", 0.9375),
	INIT_PARAM_DFLT(requestCountThreshold, "The request intensity (requests / tick) to assume no request increase", 512),
	INIT_PARAM(requestEstimationMethod, "The request estimation method to use"),
	INIT_PARAM_DFLT(acceptanceThreshold, "The performance improvement needed to accept new MHA", 1.0),
	INIT_PARAM_DFLT(requestVariationThreshold, "Maximum acceptable request variation", 0.1),
	INIT_PARAM_DFLT(renewMeasurementsThreshold, "Samples to keep MHA", 32),
	INIT_PARAM_DFLT(searchAlgorithm, "The search algorithm to use", "exhaustive"),
	INIT_PARAM_DFLT(busRequestThreshold, "The bus request intensity necessary to consider request increases", 256),
	INIT_PARAM(writeStallTechnique, "The technique to use to estimate private write stalls"),
	INIT_PARAM(privateBlockedStallTechnique, "The technique to use to estimate private blocked stalls"),
	INIT_PARAM(emptyROBStallTechnique, "The technique to use to estimate private mode empty ROB stalls"),
	INIT_PARAM_DFLT(cplCutoff, "CPL value where to cut the model damping", 50),
	INIT_PARAM_DFLT(latencyCutoff, "Latency value where to cut the model damping", 120)
END_INIT_SIM_OBJECT_PARAMS(MissBandwidthPolicy)

CREATE_SIM_OBJECT(MissBandwidthPolicy)
{
	BasePolicy::RequestEstimationMethod reqEstMethod =
		BasePolicy::parseRequestMethod(requestEstimationMethod);
	BasePolicy::PerformanceEstimationMethod perfEstMethod =
		BasePolicy::parsePerformanceMethod(performanceEstimationMethod);
	BasePolicy::SearchAlgorithm searchAlg =
			BasePolicy::parseSearchAlgorithm(searchAlgorithm);

	Metric* performanceMetric = BasePolicy::parseOptimizationMetric(optimizationMetric);

	BasePolicy::WriteStallTechnique wst = BasePolicy::parseWriteStallTech(writeStallTechnique);
	BasePolicy::PrivBlockedStallTechnique pbst = BasePolicy::parsePrivBlockedStallTech(privateBlockedStallTechnique);

	BasePolicy::EmptyROBStallTechnique rst = BasePolicy::parseEmptyROBStallTech(emptyROBStallTechnique);

	return new MissBandwidthPolicy(getInstanceName(),
							       interferenceManager,
							       period,
							       cpuCount,
							       perfEstMethod,
							       persistentAllocations,
							       iterationLatency,
							       performanceMetric,
							       enforcePolicy,
							       sharedCacheThrottle,
							       privateCacheThrottles,
							       busUtilizationThreshold,
							       requestCountThreshold,
							       reqEstMethod,
							       acceptanceThreshold,
							       requestVariationThreshold,
							       renewMeasurementsThreshold,
							       searchAlg,
							       busRequestThreshold,
							       wst,
							       pbst,
							       rst,
							       cplCutoff,
							       latencyCutoff);
}

REGISTER_SIM_OBJECT("MissBandwidthPolicy", MissBandwidthPolicy)

#endif //DOXYGEN_SHOULD_SKIP_THIS

