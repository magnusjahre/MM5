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
	busUseCycles = 0.0;

	avgMemBusParallelism = 0.0;
}

double
PerformanceModelMeasurements::getLittlesLawBusQueueLatency(){

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

	DPRINTF(PerformanceModelMeasurements, "Average number of requests in the system is %f and they spend %f ticks in the system\n",
			(busRequests*avgMemoryBusServiceLat) / (ticksInSample*bandwidthAllocation),
			avgMemoryBusServiceLat / bandwidthAllocation);

	assert(denominator > 0.0);
	double res = numerator / denominator;

	DPRINTF(PerformanceModelMeasurements, "Model queue latency %f (measured %f)\n",
			res,
			avgMemoryBusQueueLat);

	return res;

}

double
PerformanceModelMeasurements::getGraphModelBusQueueLatency(){
	double numerator = avgMemoryBusServiceLat * avgMemBusParallelism;
	double res = numerator / bandwidthAllocation;

	DPRINTF(PerformanceModelMeasurements, "Service latency %f, average bus parallelism %f and allocation %f gives queue estimate %f\n",
			avgMemoryBusServiceLat,
			avgMemBusParallelism,
			bandwidthAllocation,
			res);


	DPRINTF(PerformanceModelMeasurements, "Graph model queue latency %f (measured %f)\n",
			res,
			avgMemoryBusQueueLat);

	return res;
}

double
PerformanceModelMeasurements::getActualBusUtilization(){
	return (double) busUseCycles / (double) ticksInSample;
}
