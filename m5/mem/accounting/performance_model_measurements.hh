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

class PerformanceModelMeasurements{
public:

	int committedInstructions;
	Tick ticksInSample;

	double avgMemoryBusServiceLat;
	double avgMemoryBusQueueLat;
	int busRequests;

	double bandwidthAllocation;
	double busUseCycles;

	PerformanceModelMeasurements();

	double getModelBusQueueLatency();

	double getActualBusUtilization();

};



#endif /* PERFORMANCE_MODEL_MEASUREMENTS_HH_ */
