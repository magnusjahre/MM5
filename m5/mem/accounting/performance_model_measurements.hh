/*
 * performance_model_measurements.hh
 *
 *  Created on: Sep 19, 2014
 *      Author: jahre
 */

#ifndef PERFORMANCE_MODEL_MEASUREMENTS_HH_
#define PERFORMANCE_MODEL_MEASUREMENTS_HH_

#include "sim/sim_object.hh"
#include "base/trace.hh"
#include "base/misc.hh"
#include "mem/accounting/memory_overlap_estimator.hh"

class OverlapStatisticsHistogramEntry;

class PerformanceModelMeasurements{
public:

	typedef enum {
		BUS_MODEL_BURST,
		BUS_MODEL_SATURATED
	} BusModelType;

	int committedInstructions;
	Tick ticksInSample;

	double avgMemoryBusServiceLat;
	double avgMemoryBusQueueLat;
	int busRequests;
	int busWritebacks;

	double bandwidthAllocation;
	double busUseCycles;

	double avgMemBusParallelism;

	double cpl;

	std::vector<OverlapStatisticsHistogramEntry> memBusParaHistorgram;

	PerformanceModelMeasurements();

	double getLittlesLawBusQueueLatency();

	double getGraphModelBusQueueLatency(BusModelType type);

	double getGraphHistorgramBusQueueLatency(BusModelType type);

	double getActualBusUtilization();

private:
	double computeQueueEstimate(double burstSize, BusModelType type);

};



#endif /* PERFORMANCE_MODEL_MEASUREMENTS_HH_ */
