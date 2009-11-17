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

class PerformanceMeasurement{
private:
	int cpuCount;
	int numIntTypes;
	int maxMSHRs;

public:
	std::vector<int> committedInstructions;
	std::vector<int> requestsInSample;

	std::vector<std::vector<double> > mlpEstimate;

	std::vector<double> sharedLatencies;
	std::vector<double> estimatedPrivateLatencies;
	std::vector<std::vector<double> > latencyBreakdown;
	std::vector<std::vector<double> > privateLatencyBreakdown;

	double actualBusUtilization;
	double sharedCacheMissRate;

	PerformanceMeasurement(int _cpuCount, int _numIntTypes, int _maxMSHRs);

	void addInterferenceData(std::vector<std::vector<double> > sharedAvgLatencies,
							 std::vector<std::vector<double> > privateAvgEstimation);

	std::vector<std::string> getTraceHeader();
	std::vector<RequestTraceEntry> createTraceLine();

	double getNonMemoryCycles(int cpuID, int currentMSHRCount, int period);
	double getAloneCycles(int cpuID, int period);
};


#endif /* PERFORMANCE_MEASUREMENT_HH_ */
