/*
 * performance_measurement.hh
 *
 *  Created on: Nov 16, 2009
 *      Author: jahre
 */

#ifndef PERFORMANCE_MEASUREMENT_HH_
#define PERFORMANCE_MEASUREMENT_HH_

#include "mem/requesttrace.hh"

#include <vector>

class CacheMissMeasurements{
public:
	int misses;
	int interferenceMisses;
	int accesses;

	CacheMissMeasurements()
	: misses(0), interferenceMisses(0), accesses(0){
	}

	CacheMissMeasurements(int _misses, int _interferenceMisses, int _accesses)
	: misses(_misses), interferenceMisses(_interferenceMisses), accesses(_accesses){
	}

	void add(CacheMissMeasurements newValues){
		misses += newValues.misses;
		interferenceMisses += newValues.interferenceMisses;
		accesses += newValues.accesses;
	}

	double getInterferenceMissesPerMiss();

	double getMissRate();
};

class PerformanceMeasurement{
private:
	int cpuCount;
	int numIntTypes;
	int maxMSHRs;
	int period;

public:
	std::vector<int> committedInstructions;
	std::vector<int> requestsInSample;
	std::vector<int> responsesWhileStalled;

	std::vector<int> cpuStallCycles;

	std::vector<std::vector<double> > mlpEstimate;
	std::vector<std::vector<double> > avgMissesWhileStalled;

	std::vector<double> sharedLatencies;
	std::vector<double> estimatedPrivateLatencies;
	std::vector<std::vector<double> > latencyBreakdown;
	std::vector<std::vector<double> > privateLatencyBreakdown;

	std::vector<double> cacheInterferenceLatency;

	std::vector<double> alphas;
	std::vector<double> betas;

	double actualBusUtilization;
	double sharedCacheMissRate;

	std::vector<int> busAccessesPerCore;
	std::vector<int> busReadsPerCore;

	std::vector<CacheMissMeasurements> perCoreCacheMeasurements;

	PerformanceMeasurement(int _cpuCount, int _numIntTypes, int _maxMSHRs, int _period);

	void addInterferenceData(std::vector<double> sharedLatencyAccumulator,
						     std::vector<double> interferenceEstimateAccumulator,
							 std::vector<std::vector<double> > sharedLatencyBreakdownAccumulator,
							 std::vector<std::vector<double> > interferenceBreakdownAccumulator,
							 std::vector<int> localRequests);

	std::vector<std::string> getTraceHeader();
	std::vector<RequestTraceEntry> createTraceLine();

	double getNonStallCycles(int cpuID, int period);



	int getPeriod(){
		return period;
	}

	void updateConstants();

private:
	void updateAlpha(int cpuID);

	void updateBeta(int cpuID);
};


#endif /* PERFORMANCE_MEASUREMENT_HH_ */
