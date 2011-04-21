/*
 * model_throttling.cc
 *
 *  Created on: Jan 9, 2011
 *      Author: jahre
 */

#include "model_throttling.hh"

#include "lp_lib.h"

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

	throttleLimit = 1.0 / 10000.0; //FIXME: parameterize

	throttleTrace = RequestTrace(_name, "ThrottleTrace", 1);
	initThrottleTrace(_cpuCount);

	searchItemNum = 0;
	if(doVerification){
		modelSearchTrace = RequestTrace(_name, "ModelSearchTrace", 1);
		initSearchTrace(_cpuCount);

		registerExitCallback(new PolicyCallback(this));
	}

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

	vector<double> optimalArrivalRates;

	if(cpuCount > 1){
		optimalArrivalRates = findOptimalArrivalRates(&measurements);
		setArrivalRates(optimalArrivalRates);
	}
	else{
		assert(doVerification);
	}

	if(doVerification){
		dumpVerificationData(&measurements, optimalArrivalRates);
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
ModelThrottlingPolicy::checkConvergence(std::vector<double> xstar, std::vector<double> xvec, std::vector<double> gradient){
	double xvecval = 0.0;
	for(int i=0;i<xvec.size();i++) xvecval += gradient[i]*xvec[i];

	double xstarval = 0.0;
	for(int i=0;i<xstar.size();i++) xstarval += gradient[i]*xstar[i];

	if(xstarval == 0.0){
		DPRINTF(MissBWPolicy, "wk(xstar) %f, assuming bootstrap\n", xstarval);
		return true;
	}

	double correctionFactor = 0.0001;
	DPRINTF(MissBWPolicy, "Checking convergence, wk(xstar) %f > wk(xvec) %f (%f)\n", xstarval, xvecval, xvecval+correctionFactor);
	return xstarval > (xvecval + correctionFactor);
}

std::vector<double>
ModelThrottlingPolicy::findNewTrialPoint(std::vector<double> gradient, PerformanceMeasurement* measurements){
	lprec *lp;

	if ((lp = make_lp(0,cpuCount)) == NULL) fatal("Couldn't create LP");
	set_outputfile(lp, (char*) "lp-trace.txt");

	set_col_name(lp, 1, (char*) "CPU 0");
	set_col_name(lp, 2, (char*) "CPU 1");

	REAL* rowbuffer = (REAL*) malloc((cpuCount+1)*sizeof(REAL));
	rowbuffer[0] = 0.0; //element 0 is ignored

	for(int i=1;i<=cpuCount;i++) rowbuffer[i] = 1.0;
    if (!add_constraint(lp, rowbuffer, EQ, cpuCount*measurements->getPeriod())) fatal("Couldn't add sum constraint");

    for(int i=1;i<=cpuCount;i++) rowbuffer[i] = gradient[i-1];
    set_obj_fn(lp, rowbuffer);

    for(int i=1;i<=cpuCount;i++) set_lowbo(lp, i, aloneCycles[i-1]);

    set_maxim(lp);

    print_lp(lp);

    solve(lp);

    int intsum = floor(get_var_primalresult(lp, 1) + 0.5);
    if(intsum != cpuCount*measurements->getPeriod()){
    	fatal("Constraint constant %d does not satisfy constraint after LP-solve (should be %d)", intsum, cpuCount*measurements->getPeriod());
    }

    vector<double> retval = vector<double>(cpuCount, 0.0);
    for(int i=0;i<cpuCount;i++){
    	retval[i] = get_var_primalresult(lp, i+2);
    	DPRINTF(MissBWPolicy, "xstar for CPU %d is %f\n", i, retval[i]);
    }

    print_solution(lp, 2);

    free(rowbuffer);
    delete_lp(lp);

	return retval;
}

std::vector<double>
ModelThrottlingPolicy::addMultCons(std::vector<double> xvec, std::vector<double> xstar, double step){
	vector<double> outvec = vector<double>(xvec.size(), 0.0);
	for(int i=0;i<outvec.size();i++){
		outvec[i] = xvec[i] + step*(xstar[i] - xvec[i]);
	}
	return outvec;
}

double
ModelThrottlingPolicy::findOptimalStepSize(std::vector<double> xvec, std::vector<double> xstar, PerformanceMeasurement* measurements){

	double step = 0.0001;
	double stepsize = 0.0001;
	double endval=1.0;

	double maxstep = 0.0;
	double maxval = 0.0;

	while(step <= endval){
		double curval = performanceMetric->computeFunction(measurements, addMultCons(xvec, xstar, step), aloneCycles);
		if(curval > maxval){
			maxval = curval;
			maxstep = step;
		}
		step += stepsize;
	}

	DPRINTF(MissBWPolicy, "Best maxstep is %f\n", maxstep);
	return maxstep;
}

std::vector<double>
ModelThrottlingPolicy::findOptimalArrivalRates(PerformanceMeasurement* measurements){

	measurements->updateConstants();

	vector<double> xvals = vector<double>(cpuCount, measurements->getPeriod());
	vector<double> xstar = vector<double>(cpuCount, 0.0);
	vector<double> thisGradient = performanceMetric->gradient(measurements, aloneCycles, cpuCount, xvals);
	if(doVerification) traceSearch(xvals);
	int cutoff = 0;

	while(checkConvergence(xstar, xvals, thisGradient)){
		thisGradient = performanceMetric->gradient(measurements, aloneCycles, cpuCount, xvals);
		xstar = findNewTrialPoint(thisGradient, measurements);
		double stepsize = findOptimalStepSize(xvals, xstar, measurements);
		xvals = addMultCons(xvals, xstar, stepsize);
		traceVector("New xvector is: ", xvals);

		if(doVerification) traceSearch(xvals);

		cutoff++;
		if(cutoff > 1000){
			fatal("Linear programming technique solution did not converge\n");
		}
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
		if(optimalRequestRates[i] < throttleLimit){
			DPRINTF(MissBWPolicy, "Throttle for CPU %d is high %f, setting to limit val %d ", i, optimalRequestRates[i], throttleLimit);
			optimalRequestRates[i] = throttleLimit;
		}

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
		caches[i]->setTargetArrivalRate(rates);
	}
}

void
ModelThrottlingPolicy::initSearchTrace(int np){
	vector<string> headers;
	headers.push_back("Iteration");
	for(int i=0;i<cpuCount;i++) headers.push_back(RequestTrace::buildTraceName("CPU", i));
	modelSearchTrace.initalizeTrace(headers);
}

void
ModelThrottlingPolicy::traceSearch(std::vector<double> xvec){
	vector<RequestTraceEntry> vals = vector<RequestTraceEntry>(xvec.size()+1, 0);
	vals[0] = searchItemNum;
	for(int i=1;i<=xvec.size();i++) vals[i] = xvec[i-1];
	modelSearchTrace.addTrace(vals);
	searchItemNum++;
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
ModelThrottlingPolicy::dumpVerificationData(PerformanceMeasurement* measurements, std::vector<double> optimalArrivalRates){

	DataDump dumpvals = DataDump("throttling-data-dump.txt");
	if(cpuCount > 1){
		for(int i=0;i<cpuCount;i++) dumpvals.addElement(DataDump::buildKey("optimal-arrival-rates", i), optimalArrivalRates[i]);
		for(int i=0;i<cpuCount;i++) dumpvals.addElement(DataDump::buildKey("optimal-periods", i), (int) optimalPeriods[i]);
		for(int i=0;i<cpuCount;i++) dumpvals.addElement(DataDump::buildKey("optimal-throttles", i), 1.0 / optimalArrivalRates[i]);
		for(int i=0;i<cpuCount;i++) dumpvals.addElement(DataDump::buildKey("alone-cycles", i), RequestTraceEntry(aloneCycles[i]));
		for(int i=0;i<cpuCount;i++) dumpvals.addElement(DataDump::buildKey("alpha", i), measurements->alphas[i]);
		for(int i=0;i<cpuCount;i++) dumpvals.addElement(DataDump::buildKey("beta", i), measurements->betas[i]);
		for(int i=0;i<cpuCount;i++) dumpvals.addElement(DataDump::buildKey("measured-request-rate", i), (double) measurements->requestsInSample[i] / (double) measurements->getPeriod());

		vector<double> tmpPeriods = vector<double>(cpuCount, (double) measurements->getPeriod());
		dumpvals.addElement("cur-metric-value", performanceMetric->computeFunction(measurements, tmpPeriods, aloneCycles));
		dumpvals.addElement("opt-metric-value", performanceMetric->computeFunction(measurements, optimalPeriods, aloneCycles));
	}


	for(int i=0;i<cpuCount;i++) dumpvals.addElement(DataDump::buildKey("committed-instructions", i), measurements->committedInstructions[i]);
	for(int i=0;i<cpuCount;i++) dumpvals.addElement(DataDump::buildKey("requests", i), measurements->requestsInSample[i]);
	for(int i=0;i<cpuCount;i++) dumpvals.addElement(DataDump::buildKey("ticks", i), curTick);
	for(int i=0;i<cpuCount;i++) dumpvals.addElement(DataDump::buildKey("avg-bus-queue", i), measurements->latencyBreakdown[i][InterferenceManager::MemoryBusQueue]);
	for(int i=0;i<cpuCount;i++) dumpvals.addElement(DataDump::buildKey("avg-bus-service", i), measurements->latencyBreakdown[i][InterferenceManager::MemoryBusService]);

	dumpvals.dump();
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


