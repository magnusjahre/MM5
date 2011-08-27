/*
 * performance_measurement.cc
 *
 *  Created on: Nov 16, 2009
 *      Author: jahre
 */

#include "performance_measurement.hh"
#include "mem/interference_manager.hh" // for latency type names

using namespace std;

PerformanceMeasurement::PerformanceMeasurement(int _cpuCount, int _numIntTypes, int _maxMSHRs, int _period){

	cpuCount = _cpuCount;
	numIntTypes = _numIntTypes;
	maxMSHRs = _maxMSHRs;
	period = _period;

	committedInstructions.resize(cpuCount, 0);
	requestsInSample.resize(cpuCount, 0);
	responsesWhileStalled.resize(cpuCount, 0);
	mlpEstimate.resize(cpuCount, vector<double>());
	avgMissesWhileStalled.resize(cpuCount, vector<double>());

	sharedLatencies.resize(cpuCount, 0.0);
	estimatedPrivateLatencies.resize(cpuCount, 0.0);

	latencyBreakdown.resize(cpuCount, vector<double>(numIntTypes, 0.0));
	privateLatencyBreakdown.resize(cpuCount, vector<double>(numIntTypes, 0.0));
	cacheInterferenceLatency.resize(cpuCount, 0.0);

	actualBusUtilization = 0.0;
	sharedCacheMissRate = 0.0;

	avgBusServiceCycles = 0.0;
	otherBusRequests = 0;
	sumBusQueueCycles = 0.0;

	maxRate = 0.0;

	busAccessesPerCore.resize(cpuCount, 0);
	busReadsPerCore.resize(cpuCount, 0);

	cpuStallCycles.resize(cpuCount, 0);

	perCoreCacheMeasurements.resize(cpuCount, CacheMissMeasurements());

	alphas.resize(cpuCount, 0.0);
	betas.resize(cpuCount, 0.0);
	currentBWShare.resize(cpuCount, 0.0);

	maxReadRequestRate = 0.0;
	uncontrollableMissRequestRate = 0.0;
}

void
PerformanceMeasurement::addInterferenceData(std::vector<double> sharedLatencyAccumulator,
	                                        std::vector<double> interferenceEstimateAccumulator,
	                                        std::vector<std::vector<double> > sharedLatencyBreakdownAccumulator,
	                                        std::vector<std::vector<double> > interferenceBreakdownAccumulator,
	                                        std::vector<int> localRequests){

	for(int i=0;i<cpuCount;i++){
		if(localRequests[i] != 0){
			sharedLatencies[i] = sharedLatencyAccumulator[i] / (double) localRequests[i];

			double tmpPrivateEstimate = sharedLatencyAccumulator[i] - interferenceEstimateAccumulator[i];
			estimatedPrivateLatencies[i] = tmpPrivateEstimate / (double) localRequests[i];

			for(int j=0;j<numIntTypes;j++){
				latencyBreakdown[i][j] = sharedLatencyBreakdownAccumulator[i][j] / (double) localRequests[i];
				double tmpPrivateBreakdownEstimate = sharedLatencyBreakdownAccumulator[i][j] - interferenceBreakdownAccumulator[i][j];
				privateLatencyBreakdown[i][j] = tmpPrivateBreakdownEstimate / (double) localRequests[i];
			}

			cacheInterferenceLatency[i] = interferenceBreakdownAccumulator[i][InterferenceManager::CacheCapacity];
		}
		else{
			sharedLatencies[i] = 0;
			estimatedPrivateLatencies[i] = 0;
			for(int j=0;j<numIntTypes;j++){
				latencyBreakdown[i][j] = 0;
				privateLatencyBreakdown[i][j] = 0;				
			}
			cacheInterferenceLatency[i] = 0.0;
		}

	}
}

vector<string>
PerformanceMeasurement::getTraceHeader(){
	vector<string> header;

	for(int i=0;i<cpuCount;i++){
		stringstream name;
		name << "Committed Instructions CPU" << i;
		header.push_back(name.str());
	}

	for(int i=0;i<cpuCount;i++) header.push_back(RequestTrace::buildTraceName("IPC", i));


	for(int i=0;i<cpuCount;i++){
		stringstream name;
		name << "Requests CPU" << i;
		header.push_back(name.str());
	}

	for(int i=0;i<cpuCount;i++) header.push_back(RequestTrace::buildTraceName("Requests While Stalled", i));

	for(int i=0;i<cpuCount;i++){
		stringstream name;
		name << "Stall Cycles CPU" << i;
		header.push_back(name.str());
	}

	for(int i=0;i<cpuCount;i++){
		for(int j=0;j<maxMSHRs+1;j++){
			stringstream name;
			name << "CPU" << i << " MLP" << j;
			header.push_back(name.str());
		}
	}

	for(int i=0;i<cpuCount;i++){
		for(int j=0;j<maxMSHRs+1;j++){
			stringstream name;
			name << "CPU" << i << " Avg MWS" << j;
			header.push_back(name.str());
		}
	}

	for(int i=0;i<cpuCount;i++){
		for(int j=0;j<numIntTypes+1;j++){
			stringstream name;
			name << "CPU" << i << " Shared ";

			if(j == 0){
				name << "Total";
			}
			else{
				name << InterferenceManager::latencyStrings[j-1];
			}

			header.push_back(name.str());
		}
	}

	for(int i=0;i<cpuCount;i++){
		for(int j=0;j<numIntTypes+1;j++){
			stringstream name;
			name << "CPU" << i << " Private ";

			if(j == 0){
				name << "Total";

			}
			else{
				name << InterferenceManager::latencyStrings[j-1];
			}

			header.push_back(name.str());
		}
	}

	header.push_back("Actual Bus Utilization");
	header.push_back("Shared Cache Miss Rate");

	for(int i=0;i<cpuCount;i++) header.push_back(RequestTrace::buildTraceName("Cache Misses", i));
	for(int i=0;i<cpuCount;i++) header.push_back(RequestTrace::buildTraceName("Cache Interference Misses", i));
	for(int i=0;i<cpuCount;i++) header.push_back(RequestTrace::buildTraceName("Cache Accesses", i));

	return header;
}

vector<RequestTraceEntry>
PerformanceMeasurement::createTraceLine(){
	vector<RequestTraceEntry> line;

	for(int i=0;i<cpuCount;i++){
		line.push_back(committedInstructions[i]);
	}

	for(int i=0;i<cpuCount;i++){
		line.push_back((double) committedInstructions[i] / (double) period);
	}

	for(int i=0;i<cpuCount;i++){
		line.push_back(requestsInSample[i]);
	}

	for(int i=0;i<cpuCount;i++) line.push_back(responsesWhileStalled[i]);

	for(int i=0;i<cpuCount;i++){
		line.push_back(cpuStallCycles[i]);
	}

	for(int i=0;i<cpuCount;i++){
		for(int j=0;j<maxMSHRs+1;j++){
			line.push_back(mlpEstimate[i][j]);
		}
	}

	for(int i=0;i<cpuCount;i++){
		for(int j=0;j<maxMSHRs+1;j++){
			line.push_back(avgMissesWhileStalled[i][j]);
		}
	}

	for(int i=0;i<cpuCount;i++){
		line.push_back(sharedLatencies[i]);
		for(int j=0;j<numIntTypes;j++){
			line.push_back(latencyBreakdown[i][j]);
		}
	}

	for(int i=0;i<cpuCount;i++){
		line.push_back(estimatedPrivateLatencies[i]);
		for(int j=0;j<numIntTypes;j++){
			line.push_back(privateLatencyBreakdown[i][j]);
		}
	}

	line.push_back(actualBusUtilization);
	line.push_back(sharedCacheMissRate);

	for(int i=0;i<cpuCount;i++) line.push_back(perCoreCacheMeasurements[i].readMisses);
	for(int i=0;i<cpuCount;i++) line.push_back(perCoreCacheMeasurements[i].interferenceMisses);
	for(int i=0;i<cpuCount;i++) line.push_back(perCoreCacheMeasurements[i].accesses);

	return line;
}

double
PerformanceMeasurement::getNonStallCycles(int cpuID, int period){
	assert(period >= cpuStallCycles[cpuID]);
	return period - cpuStallCycles[cpuID];
}

void
PerformanceMeasurement::updateConstants(){
	setBandwidthBound();
	for(int i=0;i<cpuCount;i++){
		updateAlpha(i);
		updateBeta(i);

		double modelPeriod = (alphas[i] / currentBWShare[i]) + betas[i];
		DPRINTF(MissBWPolicy, "Model Calibration: model period is %f, actual period %d, error %f, alphas/bwshare=%f\n",
				modelPeriod,
				period,
				(modelPeriod - period)/(double)period,
				alphas[i] / currentBWShare[i]);
	}
}

void
PerformanceMeasurement::updateAlpha(int cpuID){

	DPRINTF(MissBWPolicy, "Computed average bus service latency %f for CPU %d\n", avgBusServiceCycles, cpuID);

	DPRINTF(MissBWPolicy, "CPU %d had %d cache misses, %d writeback misses and caused %d shared cache writebacks (total requests %d)\n",
			cpuID,
			perCoreCacheMeasurements[cpuID].readMisses,
			perCoreCacheMeasurements[cpuID].wbMisses,
			perCoreCacheMeasurements[cpuID].sharedCacheWritebacks,
			requestsInSample[cpuID]);

	double totalMisses = 0;
	for(int i=0;i<cpuCount;i++){
		totalMisses += perCoreCacheMeasurements[i].readMisses + perCoreCacheMeasurements[i].wbMisses + perCoreCacheMeasurements[i].sharedCacheWritebacks;
	}

	currentBWShare[cpuID] = perCoreCacheMeasurements[cpuID].readMisses / totalMisses;
	DPRINTF(MissBWPolicy, "CPU %d has a %f share of the total bus bandwidth\n", cpuID, currentBWShare[cpuID]);

	if(avgBusServiceCycles == 0.0){
		assert(totalMisses == 0);
		DPRINTF(MissBWPolicy, "No requests used the bus, alpha is 0\n");
		alphas[cpuID] = 0.0;
		return;
	}
	assert(avgBusServiceCycles > 0.0);

	double thisMisses = perCoreCacheMeasurements[cpuID].readMisses;
	if(perCoreCacheMeasurements[cpuID].readMisses == 0){
		DPRINTF(MissBWPolicy, "No read misses for CPU %d used the bus, alpha is 0\n", cpuID);
		alphas[cpuID] = 0.0;
		return;
	}
	assert(perCoreCacheMeasurements[cpuID].readMisses >= 0);

	double overlap = computedOverlap[cpuID];

	DPRINTF(MissBWPolicy, "CPU %d: Overlap %f, this core misses %f, total misses %f \n", cpuID, overlap, thisMisses, totalMisses);

	double avgWaitCalPerReq = latencyBreakdown[cpuID][InterferenceManager::MemoryBusQueue]
	                        + latencyBreakdown[cpuID][InterferenceManager::MemoryBusEntry]
	                        + latencyBreakdown[cpuID][InterferenceManager::MemoryBusService];

	double avgWaitCalTot = avgWaitCalPerReq * requestsInSample[cpuID];
	double avgWaitPerMiss = avgWaitCalTot / thisMisses;

	double curRate = perCoreCacheMeasurements[cpuID].readMisses / (double) period;
	double curBWShare = curRate / maxReadRequestRate;

	double avgMinWaitPerMiss = curBWShare * avgWaitPerMiss;

	alphas[cpuID] = overlap * thisMisses * avgMinWaitPerMiss;

	DPRINTF(MissBWPolicy, "Current BW share is %f, estimated average wait time per miss is %f, estimated minimum wait time per miss is %f, returning alpha %d\n",
			curBWShare,
			avgWaitPerMiss,
			avgMinWaitPerMiss,
			alphas[cpuID]);
}

void
PerformanceMeasurement::setBandwidthBound(){

	double useAvgBusCycles = avgBusServiceCycles;
	if(avgBusServiceCycles == 0.0){
		DPRINTF(MissBWPolicy, "No requests in previous sample, falling back to 120 clock cycles\n");
		useAvgBusCycles = 120.0;
	}

	assert(useAvgBusCycles > 0.0);
	maxRate = 1/useAvgBusCycles;
	DPRINTF(MissBWPolicy, "Estimated maximum request rate the bus can handle is %f\n", maxRate);

	double uncontrollableMisses = 0.0;
	for(int i=0;i<cpuCount;i++){
		uncontrollableMisses += perCoreCacheMeasurements[i].wbMisses + perCoreCacheMeasurements[i].sharedCacheWritebacks;
	}
	uncontrollableMissRequestRate = uncontrollableMisses / (double) period;

	maxReadRequestRate = maxRate - uncontrollableMissRequestRate;
	DPRINTF(MissBWPolicy, "There are %f misses the model cannot reach (rate %f), returning reachable max %f\n", uncontrollableMisses, uncontrollableMissRequestRate, maxReadRequestRate);

}

void
PerformanceMeasurement::updateBeta(int cpuID){
	double ic = 0.0;
	ic += latencyBreakdown[cpuID][InterferenceManager::InterconnectDelivery];
	ic += latencyBreakdown[cpuID][InterferenceManager::InterconnectEntry];
	ic += latencyBreakdown[cpuID][InterferenceManager::InterconnectRequestQueue];
	ic += latencyBreakdown[cpuID][InterferenceManager::InterconnectRequestTransfer];
	ic += latencyBreakdown[cpuID][InterferenceManager::InterconnectResponseQueue];
	ic += latencyBreakdown[cpuID][InterferenceManager::InterconnectResponseTransfer];

	double cache = latencyBreakdown[cpuID][InterferenceManager::CacheCapacity];

	double compute = getNonStallCycles(cpuID, period);

	double overlap = computedOverlap[cpuID];

	DPRINTF(MissBWPolicy, "CPU %d has %f compute cycles, overlap %f and non-bus cycles %f (average %f)\n",
			cpuID,
			compute,
			overlap,
			requestsInSample[cpuID] * (ic + cache),
			ic + cache);

	betas[cpuID] = compute + overlap * requestsInSample[cpuID] * (ic + cache);

	DPRINTF(MissBWPolicy, "Computed beta %f for CPU %d\n", betas[cpuID], cpuID);
}

double
CacheMissMeasurements::getInterferenceMissesPerMiss(){

	double dblIntMiss = (double) interferenceMisses;
	double dblMisses = (double) readMisses;

	double intMissRate = -1.0;
	if(interferenceMisses > readMisses) intMissRate = 1.0;
	else if(readMisses == 0) intMissRate = 0.0;
	else intMissRate = dblIntMiss / dblMisses;

	assert(intMissRate >= 0.0 && intMissRate <= 1.0);

	return intMissRate;
}

double
CacheMissMeasurements::getMissRate(){
	double dblAccesses = (double) accesses;
	double dblMisses = (double) readMisses;

	if(accesses == 0){
		assert(readMisses == 0);
		return 0.0;
	}

	double missRate = dblMisses / dblAccesses;
	assert(missRate >= 0.0 && missRate <= 1.0);
	return missRate;
}

void
CacheMissMeasurements::printMissCurve(){
	for(int i=0;i<privateCumulativeCacheMisses.size();i++){
		cout << "("<< i << ", " << privateCumulativeCacheMisses[i] << ") ";
	}
	cout << "\n";
}

