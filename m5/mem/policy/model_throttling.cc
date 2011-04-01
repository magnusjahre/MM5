/*
 * model_throttling.cc
 *
 *  Created on: Jan 9, 2011
 *      Author: jahre
 */

#include "model_throttling.hh"

ModelThrottlingPolicy::ModelThrottlingPolicy(std::string _name,
			   	    			 InterferenceManager* _intManager,
			   	    			 Tick _period,
			   	    			 int _cpuCount,
			   	    			 PerformanceEstimationMethod _perfEstMethod,
			   	    			 bool _persistentAllocations,
			   	    			 int _iterationLatency,
			   	    			 Metric* _performanceMetric,
			   	    			 bool _enforcePolicy,
			   	    			 bool _verify,
			   	    			 std::vector<double> _staticArrivalRates)
: BasePolicy(_name, _intManager, _period, _cpuCount, _perfEstMethod, _persistentAllocations, _iterationLatency, _performanceMetric, _enforcePolicy)
{
	//enableOccupancyTrace = true;

	doVerification = _verify;

	optimalPeriods.resize(_cpuCount, 0.0);

	throttleTrace = RequestTrace(_name, "ThrottleTrace", 1);
	initThrottleTrace(_cpuCount);

	if(!_staticArrivalRates.empty()){
		StaticAllocationEvent* event = new StaticAllocationEvent(this, _staticArrivalRates);
		event->schedule(1);
	}
}

void
ModelThrottlingPolicy::runPolicy(PerformanceMeasurement measurements){

	DPRINTF(MissBWPolicy, "--- Running Model Throttling Policy\n");

	assert(currentMeasurements == NULL);
	currentMeasurements = &measurements;

	updateMWSEstimates();
	updateAloneIPCEstimate();
	measurements.computedOverlap = computedOverlap;
	traceVector("Alone IPC Estimates: ", aloneIPCEstimates);
	traceVector("Estimated Alone Cycles: ", aloneCycles);
	traceVector("Computed Overlap: ", computedOverlap);

	vector<double> optimalArrivalRates = findOptimalArrivalRates(&measurements);
	setArrivalRates(optimalArrivalRates);

	if(doVerification){
		quitForVerification(&measurements, optimalArrivalRates);
	}

	currentMeasurements = NULL;
}

bool
ModelThrottlingPolicy::doEvaluation(int cpuID){
	fatal("model throttling doEvaluation not implemented");
	return false;
}

//std::vector<double>
//ModelThrottlingPolicy::findOptimalArrivalRates(PerformanceMeasurement* measurements){
//	optimalPeriods = performanceMetric->computeOptimalPeriod(measurements, aloneCycles, cpuCount);
//
//	vector<double> optimalRequestRates = vector<double>(cpuCount, 0.0);
//
//	assert(optimalPeriods.size() == optimalRequestRates.size());
//	for(int i=0;i<optimalPeriods.size();i++){
//		optimalRequestRates[i] = ((double) measurements->requestsInSample[i]) / optimalPeriods[i];
//	}
//
//	traceVector("Got optimal periods: ", optimalPeriods);
//	traceVector("Request count: ", measurements->requestsInSample);
//	traceVector("Returning optimal request rates: ", optimalRequestRates);
//
//	return optimalRequestRates;
//}

double
ModelThrottlingPolicy::magnitudeOfDifference(std::vector<double> oldvec, std::vector<double> newvec){
	for(int i=0;i<newvec.size();i++){
		newvec[i] = newvec[i] - oldvec[i];
		newvec[i] = newvec[i] * newvec[i];
	}

	double magnitude = 0.0;
	for(int i=0;i<newvec.size();i++) magnitude += newvec[i];
	return sqrt(magnitude);
}

std::vector<double>
ModelThrottlingPolicy::findOptimalArrivalRates(PerformanceMeasurement* measurements){

	measurements->updateConstants();

	vector<double> oldvals = vector<double>(cpuCount+1, 0.0);
	vector<double> xvals = vector<double>(cpuCount+1, measurements->getPeriod());
	xvals[cpuCount] = performanceMetric->getInitLambda(measurements, aloneCycles, xvals[0]);
	double stepsize = 100.0;
	double presicion = 0.0000001;
	bool hitConstraint = false;
	int cutoff = 0;

	while(magnitudeOfDifference(oldvals, xvals) > presicion || hitConstraint){
		oldvals = xvals;
		vector<double> thisGradient = performanceMetric->gradient(measurements, aloneCycles, cpuCount, xvals);
		for(int i=0;i<xvals.size();i++){
			xvals[i] = oldvals[i] - stepsize * thisGradient[i];
			DPRINTF(MissBWPolicy, "New xval for CPU %d is %f, old was %f\n", i, xvals[i], oldvals[i]);
		}

		for(int i=0;i<xvals.size()-1;i++){
			if(xvals[i] < aloneCycles[i]){
				DPRINTF(MissBWPolicy, "Hit constraint for CPU %d new val %f is greater than %f\n", i, xvals[i], aloneCycles[i]);
				hitConstraint = true;
				xvals = oldvals;
			}
		}
		cutoff++;
		assert(cutoff < 10);

		DPRINTF(MissBWPolicy, "Vector magnitude is %f, precision is %f\n", magnitudeOfDifference(oldvals, xvals), presicion);
	}

	double sum = 0.0;
	for(int i=0;i<cpuCount;i++){
		optimalPeriods[i] = xvals[i];
		sum += xvals[i];
	}
	int intsum = floor(sum + 0.5);
	if(intsum != cpuCount*measurements->getPeriod()){
		fatal("Sum of optimal periods %d does not satisfy constraint (should be %d)", intsum, cpuCount*measurements->getPeriod());
	}


	vector<double> optimalRequestRates = vector<double>(cpuCount, 0.0);

	assert(optimalPeriods.size() == optimalRequestRates.size());
	for(int i=0;i<optimalPeriods.size();i++){
		optimalRequestRates[i] = ((double) measurements->requestsInSample[i]) / optimalPeriods[i];
	}

	traceVector("Got optimal periods: ", optimalPeriods);
	traceVector("Request count: ", measurements->requestsInSample);
	traceVector("Returning optimal request rates: ", optimalRequestRates);

	return optimalRequestRates;
}

void
ModelThrottlingPolicy::setArrivalRates(std::vector<double> rates){

	vector<double> cyclesPerReq = vector<double>(rates.size(), 0.0);
	for(int i=0;i<rates.size();i++){
		cyclesPerReq[i] = 1.0 / rates[i];
	}

	traceVector("Optimal cycles per request: ", cyclesPerReq);
	traceThrottling(cyclesPerReq);

	for(int i=0;i<caches.size();i++){
		DPRINTF(MissBWPolicy, "Setting target arrival rate for CPU %d to %f\n", i, rates[i]);
		caches[i]->setTargetArrivalRate(rates[i]);
	}
}

void
ModelThrottlingPolicy::initThrottleTrace(int np){
	vector<string> headers;
	for(int i=0;i<cpuCount;i++) headers.push_back(RequestTrace::buildTraceName("CPU", i));
	throttleTrace.initalizeTrace(headers);
}

void
ModelThrottlingPolicy::traceThrottling(std::vector<double> throttles){
	vector<RequestTraceEntry> vals = vector<RequestTraceEntry>(throttles.size(), 0);
	for(int i=0;i<throttles.size();i++) vals[i] = (int) throttles[i];
	throttleTrace.addTrace(vals);
}

void
ModelThrottlingPolicy::quitForVerification(PerformanceMeasurement* measurements, std::vector<double> optimalArrivalRates){

	DataDump dumpvals = DataDump("throttling-data-dump.txt");
	for(int i=0;i<cpuCount;i++) dumpvals.addElement(DataDump::buildKey("optimal-arrival-rates", i), optimalArrivalRates[i]);
	for(int i=0;i<cpuCount;i++) dumpvals.addElement(DataDump::buildKey("optimal-periods", i), (int) optimalPeriods[i]);
	for(int i=0;i<cpuCount;i++) dumpvals.addElement(DataDump::buildKey("optimal-throttles", i), 1.0 / optimalArrivalRates[i]);
	for(int i=0;i<cpuCount;i++) dumpvals.addElement(DataDump::buildKey("alone-cycles", i), RequestTraceEntry(aloneCycles[i]));
	for(int i=0;i<cpuCount;i++) dumpvals.addElement(DataDump::buildKey("alpha", i), measurements->alphas[i]);
	for(int i=0;i<cpuCount;i++) dumpvals.addElement(DataDump::buildKey("beta", i), measurements->betas[i]);
	for(int i=0;i<cpuCount;i++) dumpvals.addElement(DataDump::buildKey("committed-instructions", i), measurements->committedInstructions[i]);
	for(int i=0;i<cpuCount;i++) dumpvals.addElement(DataDump::buildKey("requests", i), measurements->requestsInSample[i]);
	for(int i=0;i<cpuCount;i++) dumpvals.addElement(DataDump::buildKey("measured-request-rate", i), (double) measurements->requestsInSample[i] / (double) measurements->getPeriod());
	for(int i=0;i<cpuCount;i++) dumpvals.addElement(DataDump::buildKey("avg-bus-queue", i), measurements->latencyBreakdown[i][InterferenceManager::MemoryBusQueue]);
	for(int i=0;i<cpuCount;i++) dumpvals.addElement(DataDump::buildKey("avg-bus-service", i), measurements->latencyBreakdown[i][InterferenceManager::MemoryBusService]);

	dumpvals.dump();

	new SimExitEvent("Stopping for model verification");
}

#ifndef DOXYGEN_SHOULD_SKIP_THIS

BEGIN_DECLARE_SIM_OBJECT_PARAMS(ModelThrottlingPolicy)
	SimObjectParam<InterferenceManager* > interferenceManager;
	Param<Tick> period;
	Param<int> cpuCount;
	Param<string> performanceEstimationMethod;
	Param<bool> persistentAllocations;
	Param<int> iterationLatency;
	Param<string> optimizationMetric;
	Param<bool> enforcePolicy;
	Param<bool> verify;
	VectorParam<double> staticArrivalRates;
END_DECLARE_SIM_OBJECT_PARAMS(ModelThrottlingPolicy)

BEGIN_INIT_SIM_OBJECT_PARAMS(ModelThrottlingPolicy)
	INIT_PARAM_DFLT(interferenceManager, "The system's interference manager" , NULL),
	INIT_PARAM_DFLT(period, "The number of clock cycles between each decision", 1048576),
	INIT_PARAM(cpuCount, "The number of cpus in the system"),
	INIT_PARAM(performanceEstimationMethod, "The method to use for performance estimations"),
	INIT_PARAM_DFLT(persistentAllocations, "The method to use for performance estimations", true),
	INIT_PARAM_DFLT(iterationLatency, "The number of cycles it takes to evaluate one MHA", 0),
	INIT_PARAM_DFLT(optimizationMetric, "The metric to optimize for", "hmos"),
	INIT_PARAM_DFLT(enforcePolicy, "Should the policy be enforced?", true),
	INIT_PARAM_DFLT(verify, "Verify policy", false),
	INIT_PARAM_DFLT(staticArrivalRates, "Static arrival rates to enforce", vector<double>())
END_INIT_SIM_OBJECT_PARAMS(ModelThrottlingPolicy)

CREATE_SIM_OBJECT(ModelThrottlingPolicy)
{

	BasePolicy::PerformanceEstimationMethod perfEstMethod =
		BasePolicy::parsePerformanceMethod(performanceEstimationMethod);

	Metric* performanceMetric = BasePolicy::parseOptimizationMetric(optimizationMetric);

	return new ModelThrottlingPolicy(getInstanceName(),
							         interferenceManager,
							         period,
							         cpuCount,
							         perfEstMethod,
							         persistentAllocations,
							         iterationLatency,
							         performanceMetric,
							         enforcePolicy,
							         verify,
							         staticArrivalRates);
}

REGISTER_SIM_OBJECT("ModelThrottlingPolicy", ModelThrottlingPolicy)

#endif //DOXYGEN_SHOULD_SKIP_THIS


