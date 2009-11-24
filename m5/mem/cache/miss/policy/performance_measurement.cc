/*
 * performance_measurement.cc
 *
 *  Created on: Nov 16, 2009
 *      Author: jahre
 */

#include "performance_measurement.hh"
#include "mem/interference_manager.hh" // for latency type names

using namespace std;

PerformanceMeasurement::PerformanceMeasurement(int _cpuCount, int _numIntTypes, int _maxMSHRs){

	cpuCount = _cpuCount;
	numIntTypes = _numIntTypes;
	maxMSHRs = _maxMSHRs;

	committedInstructions.resize(cpuCount, 0);
	requestsInSample.resize(cpuCount, 0);
	mlpEstimate.resize(cpuCount, vector<double>());

	sharedLatencies.resize(cpuCount, 0.0);
	estimatedPrivateLatencies.resize(cpuCount, 0.0);

	latencyBreakdown.resize(cpuCount, vector<double>(numIntTypes, 0.0));
	privateLatencyBreakdown.resize(cpuCount, vector<double>(numIntTypes, 0.0));

	actualBusUtilization = 0.0;
	sharedCacheMissRate = 0.0;
}

void
PerformanceMeasurement::addInterferenceData(std::vector<double> sharedLatencyAccumulator,
	                                        std::vector<double> interferenceEstimateAccumulator,
	                                        std::vector<std::vector<double> > sharedLatencyBreakdownAccumulator,
	                                        std::vector<std::vector<double> > interferenceBreakdownAccumulator,
	                                        std::vector<int> localRequests){

	for(int i=0;i<cpuCount;i++){
		sharedLatencies[i] = sharedLatencyAccumulator[i] / (double) localRequests[i];

		double tmpPrivateEstimate = sharedLatencyAccumulator[i] - interferenceEstimateAccumulator[i];
		estimatedPrivateLatencies[i] = tmpPrivateEstimate / (double) localRequests[i];

		for(int j=0;j<numIntTypes;j++){
			latencyBreakdown[i][j] = sharedLatencyBreakdownAccumulator[i][j] / (double) localRequests[i];
			double tmpPrivateBreakdownEstimate = sharedLatencyBreakdownAccumulator[i][j] - interferenceBreakdownAccumulator[i][j];
			privateLatencyBreakdown[i][j] = tmpPrivateBreakdownEstimate / (double) localRequests[i];
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
	for(int i=0;i<cpuCount;i++){
		stringstream name;
		name << "Requests CPU" << i;
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

	return header;
}

vector<RequestTraceEntry>
PerformanceMeasurement::createTraceLine(){
	vector<RequestTraceEntry> line;

	for(int i=0;i<cpuCount;i++){
		line.push_back(committedInstructions[i]);
	}

	for(int i=0;i<cpuCount;i++){
		line.push_back(requestsInSample[i]);
	}

	for(int i=0;i<cpuCount;i++){
		for(int j=0;j<maxMSHRs+1;j++){
			line.push_back(mlpEstimate[i][j]);
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

	return line;
}

double
PerformanceMeasurement::getNonMemoryCycles(int cpuID, int currentMSHRCount, int period){
	double totalLatencyCycles = (mlpEstimate[cpuID][currentMSHRCount] * sharedLatencies[cpuID]) * (double) requestsInSample[cpuID];
	if(period < totalLatencyCycles) return 0;
	return period - totalLatencyCycles;
}

double
PerformanceMeasurement::getAloneCycles(int cpuID, int period){
	double totalInterferenceCycles = (sharedLatencies[cpuID] - estimatedPrivateLatencies[cpuID]) * requestsInSample[cpuID];
	double visibleIntCycles = mlpEstimate[cpuID][maxMSHRs] * totalInterferenceCycles;
	DPRINTF(MissBWPolicyExtra, "Estimating alone cycles for CPU %d, visible interference is %f, period %d\n", cpuID, visibleIntCycles, period);
	if(visibleIntCycles > period) return 0;
	return period - visibleIntCycles;
}
