/*
 * performance_model_measurements.cc
 *
 *  Created on: Sep 19, 2014
 *      Author: jahre
 */

#include "performance_model_measurements.hh"

PerformanceModelMeasurements::PerformanceModelMeasurements(){

	committedInstructions = 0;
	ticksInSample = 0;

	avgMemoryBusServiceLat = 0.0;
	avgMemoryBusQueueLat = 0.0;
	busRequests = 0;

	bandwidthAllocation = 0.0;
}

double
PerformanceModelMeasurements::getModelBusQueueLatency(){

	double latSq = avgMemoryBusServiceLat*avgMemoryBusServiceLat;
	double numerator = (double) busRequests * latSq;

	DPRINTF(PerformanceModelMeasurements, "Bus latency %f squared is %f, requests %d, numerator %f\n",
			avgMemoryBusServiceLat,
			latSq,
			busRequests,
			numerator);

	double allocSq = bandwidthAllocation * bandwidthAllocation;
	double denominator = ticksInSample * allocSq;

	DPRINTF(PerformanceModelMeasurements, "Allocation %f squared is %f, ticks in sample %d, denominator %f\n",
			bandwidthAllocation,
			allocSq,
			ticksInSample,
			denominator);

	assert(denominator > 0.0);
	double res = numerator / denominator;

	return res;

}
