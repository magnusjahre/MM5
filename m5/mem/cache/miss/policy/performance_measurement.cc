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

	busAccessesPerCore.resize(cpuCount, 0);
	busReadsPerCore.resize(cpuCount, 0);

	cpuStallCycles.resize(cpuCount, 0);

	perCoreCacheMeasurements.resize(cpuCount, CacheMissMeasurements());
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

	for(int i=0;i<cpuCount;i++) line.push_back(perCoreCacheMeasurements[i].misses);
	for(int i=0;i<cpuCount;i++) line.push_back(perCoreCacheMeasurements[i].interferenceMisses);
	for(int i=0;i<cpuCount;i++) line.push_back(perCoreCacheMeasurements[i].accesses);

	return line;
}

double
PerformanceMeasurement::getNonStallCycles(int cpuID, int period){
	assert(period >= cpuStallCycles[cpuID]);
	return period - cpuStallCycles[cpuID];
}

double
CacheMissMeasurements::getInterferenceMissesPerMiss(){

	double dblIntMiss = (double) interferenceMisses;
	double dblMisses = (double) misses;

	double intMissRate = -1.0;
	if(interferenceMisses > misses) intMissRate = 1.0;
	else if(misses == 0) intMissRate = 0.0;
	else intMissRate = dblIntMiss / dblMisses;

	assert(intMissRate >= 0.0 && intMissRate <= 1.0);

	return intMissRate;
}
