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
	busWritebacks = 0;

	bandwidthAllocation = 0.0;
	busUseCycles = 0.0;

	cpl = 0.0;
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
PerformanceModelMeasurements::computeQueueEstimate(double burstSize, BusModelType type){

	double numerator = avgMemoryBusServiceLat * (burstSize-1.0);
	if(numerator < 0) numerator = 0.0;
	double res = 0;
	if(type == BUS_MODEL_BURST){
		res = numerator / (2.0*bandwidthAllocation);
	}
	else{
		assert(type == BUS_MODEL_SATURATED);
		res = numerator / bandwidthAllocation;
	}

	DPRINTF(PerformanceModelMeasurements, "Service latency %f, burst size %f and allocation %f gives queue estimate %f\n",
			avgMemoryBusServiceLat,
			burstSize,
			bandwidthAllocation,
			res);
	return res;
}

double
PerformanceModelMeasurements::getGraphModelBusQueueLatency(BusModelType type){

	double wbPara = busWritebacks / cpl;

	double res = computeQueueEstimate(avgMemBusParallelism+wbPara, type);

	DPRINTF(PerformanceModelMeasurements, "Graph model queue latency %f (measured %f) with bus para %f and wb para %f\n",
			res,
			avgMemoryBusQueueLat,
			avgMemBusParallelism,
			wbPara);

	return res;
}

double
PerformanceModelMeasurements::getGraphHistorgramBusQueueLatency(BusModelType type){
	double burstClasses = 0.0;
	double queueLatSum = 0.0;

	for(int i=0;i<memBusParaHistorgram.size();i++){
		DPRINTF(PerformanceModelMeasurements, "Histogram model: %d bursts of %d requests\n",
				memBusParaHistorgram[i].numBursts,
				memBusParaHistorgram[i].parallelism());
		double burstQueueLat = computeQueueEstimate(memBusParaHistorgram[i].parallelism(), type);
		queueLatSum += burstQueueLat*memBusParaHistorgram[i].numBursts;
		burstClasses += memBusParaHistorgram[i].numBursts;
	}
	if(burstClasses == 0.0){
		DPRINTF(PerformanceModelMeasurements, "Histogram model: no classes, returning 0\n");
		return 0.0;
	}

	double res = queueLatSum / burstClasses;

	DPRINTF(PerformanceModelMeasurements, "Histogram model queue latency %f (measured %f)\n",
			res,
			avgMemoryBusQueueLat);

	return res;
}

double
PerformanceModelMeasurements::getActualBusUtilization(){
	return (double) busUseCycles / (double) ticksInSample;
}
