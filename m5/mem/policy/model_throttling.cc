/*
 * model_throttling.cc
 *
 *  Created on: Jan 9, 2011
 *      Author: jahre
 */

#include "model_throttling.hh"

#include "lp_lib.h"

#define SEARCH_DECIMALS 12
#define METRIC_DECIMALS 4

ModelThrottlingPolicy::ModelThrottlingPolicy(std::string _name,
			   	    			 InterferenceManager* _intManager,
			   	    			 Tick _period,
			   	    			 int _cpuCount,
			   	    			 PerformanceEstimationMethod _perfEstMethod,
			   	    			 bool _persistentAllocations,
			   	    			 int _iterationLatency,
			   	    			 Metric* _performanceMetric,
			   	    			 bool _enforcePolicy,
			   	    			 ThrottleControl* _sharedCacheThrottle,
			   	    			 std::vector<ThrottleControl* > _privateCacheThrottles,
			   	    			 bool _verify,
			   	    			 std::vector<double> _staticArrivalRates,
			   	    			 std::string _implStrategy)
: BasePolicy(_name, _intManager, _period, _cpuCount, _perfEstMethod, _persistentAllocations, _iterationLatency, _performanceMetric, _enforcePolicy, _sharedCacheThrottle, _privateCacheThrottles)
{
	//enableOccupancyTrace = true;

	doVerification = _verify;

	optimalPeriods.resize(_cpuCount, 0.0);
	optimalBWShares = vector<double>(cpuCount+1, 0.0);

	modelValueTrace = RequestTrace(_name, "ModelValueTrace", 1);
	initModelValueTrace(_cpuCount);

	if(_implStrategy == "nfq"){
		implStrat = BW_IMPL_NFQ;
	}
	else if(_implStrategy == "throttle"){
		implStrat = BW_IMPL_THROTTLE;
	}
	else if(_implStrategy == "fixedbw"){
		implStrat = BW_IMPL_FIXED_BW;
	}
	else{
		fatal("Unknown bandwidth allocation implementation strategy");
	}

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

	assert(measurements.getPeriod() == period);

	updateMWSEstimates();
	updateAloneIPCEstimate();
	measurements.computedOverlap = computedOverlap;
	traceVector("Alone IPC Estimates: ", aloneIPCEstimates);
	traceVector("Estimated Alone Cycles: ", aloneCycles);
	traceVector("Computed Overlap: ", computedOverlap);

	vector<double> optimalArrivalRates;

	if(cpuCount > 1){
		optimalArrivalRates = findOptimalArrivalRates(&measurements);
		implementAllocation(optimalArrivalRates, measurements.getUncontrollableMissReqRate());

		traceModelValues(&measurements, optimalArrivalRates);
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
		DPRINTF(MissBWPolicyExtra, "wk(xstar) %f, assuming bootstrap\n", xstarval);
		return true;
	}

	xstarval = setPrecision(xstarval, METRIC_DECIMALS);
	xvecval = setPrecision(xvecval, METRIC_DECIMALS);

	DPRINTF(MissBWPolicyExtra, "Checking convergence, wk(xstar) %f > wk(xvec) %f\n", xstarval, xvecval);
	return xstarval > xvecval;
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
    //if (!add_constraint(lp, rowbuffer, EQ, cpuCount*period)) fatal("Couldn't add sum constraint");
	if (!add_constraint(lp, rowbuffer, LE, measurements->maxRequestRate)) fatal("Couldn't add sum constraint");

    for(int i=1;i<=cpuCount;i++) rowbuffer[i] = gradient[i-1];
    set_obj_fn(lp, rowbuffer);

    for(int i=1;i<=cpuCount;i++){
    	double upbo = (double) measurements->perCoreCacheMeasurements[i-1].readMisses / (double) aloneCycles[i-1];
    	set_upbo(lp, i, upbo);
    }

    set_maxim(lp);

    print_lp(lp);

    solve(lp);

    vector<double> retval = vector<double>(cpuCount, 0.0);
    for(int i=0;i<cpuCount;i++){
    	retval[i] = get_var_primalresult(lp, i+2);
    }

    traceVerboseVector("xstar is: ", retval);

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

	DPRINTF(MissBWPolicyExtra, "Best maxstep is %f\n", maxstep);
	return maxstep;
}

double
ModelThrottlingPolicy::setPrecision(double number, int decimalPlaces){
        double power = pow((double) 10.0, decimalPlaces);
	return floor((number * power) + 0.5) / power;
}

double
ModelThrottlingPolicy::fastFindOptimalStepSize(std::vector<double> xvec, std::vector<double> xstar, PerformanceMeasurement* measurements){
	double precision = 0.00000001;
	double xl = precision;
	double xr = 1.0;
	double x1 = xl + (xr - xl - precision)/2;
	double x2 = xl + (xr - xl + precision)/2;

	double x1val = setPrecision(performanceMetric->computeFunction(measurements, addMultCons(xvec, xstar, x1), aloneCycles), SEARCH_DECIMALS);
	double x2val = setPrecision(performanceMetric->computeFunction(measurements, addMultCons(xvec, xstar, x2), aloneCycles), SEARCH_DECIMALS);

	DPRINTF(MissBWPolicyExtra, "Initial: x1=%f, x2=%f, x1val=%f, x2val=%f\n", x1, x2, x1val, x2val);

	int itcnt = 0;

	while(x1val != x2val){

		if(x1val < x2val) xl = x1;
		else if(x1val > x2val) xr = x2;
		else fatal("equal in dict search, should not happen");

		assert(xl > 0.0);
		assert(xr <= 1.0);

		x1 = xl + (xr - xl - precision)/2;
		x2 = xl + (xr - xl + precision)/2;
		x1val = setPrecision(performanceMetric->computeFunction(measurements, addMultCons(xvec, xstar, x1), aloneCycles), SEARCH_DECIMALS);
		x2val = setPrecision(performanceMetric->computeFunction(measurements, addMultCons(xvec, xstar, x2), aloneCycles), SEARCH_DECIMALS);

		DPRINTF(MissBWPolicyExtra, "x1=%f, x2=%f, x1val=%f, x2val=%f\n", x1, x2, x1val, x2val);

		itcnt++;
		if(itcnt > 50){
			DPRINTF(MissBWPolicyExtra, "Reached iteration cutoff, stopping search\n");
			x1val = x2val;
		}
	}

	double retval = (x1 + x2) / 2;
	DPRINTF(MissBWPolicyExtra, "Best maxstep is %f\n", retval);
	return retval;
}

std::vector<double>
ModelThrottlingPolicy::findOptimalArrivalRates(PerformanceMeasurement* measurements){

	measurements->updateConstants();

	vector<double> xvals = vector<double>(cpuCount, 0.0);

	vector<double> xstar = vector<double>(cpuCount, 0.0);
	vector<double> thisGradient = performanceMetric->gradient(measurements, aloneCycles, cpuCount, xvals);

	traceVector("Initial xvector is: ", xvals);
	traceVector("Initial gradient is: ", thisGradient);

	double gradsum = 0;
	for(int i=0;i<thisGradient.size();i++) gradsum += thisGradient[i];
	if(gradsum == 0){
		DPRINTF(MissBWPolicy, "All gradient values are zero, returning no allocation\n");
		return vector<double>(cpuCount, -1.0);
	}


	if(doVerification) traceSearch(xvals);
	int cutoff = 0;
	bool quitForCutoff = false;

	while(checkConvergence(xstar, xvals, thisGradient) && !quitForCutoff){
		thisGradient = performanceMetric->gradient(measurements, aloneCycles, cpuCount, xvals);
		traceVerboseVector("New gradient is: ", thisGradient);
		xstar = findNewTrialPoint(thisGradient, measurements);
		//double stepsize = findOptimalStepSize(xvals, xstar, measurements);
		double stepsize = fastFindOptimalStepSize(xvals, xstar, measurements);
		xvals = addMultCons(xvals, xstar, stepsize);
		traceVerboseVector("New xvector is: ", xvals);

		if(doVerification) traceSearch(xvals);

		cutoff++;
		if(cutoff > 5000){
			warn("Linear programming technique solution did not converge\n");
			quitForCutoff = true;
		}
	}

	traceVector("Optimal request rates are: ", xvals);

	double maxbw = measurements->maxRequestRate + measurements->getUncontrollableMissReqRate();
	for(int i=0;i<cpuCount;i++) optimalBWShares[i] = xvals[i] / maxbw;
	optimalBWShares[cpuCount] = measurements->getUncontrollableMissReqRate() / maxbw;
	traceVector("Optimal bandwidth shares are: ", optimalBWShares);

	if(implStrat == BW_IMPL_THROTTLE){
		double minimalAllocation = 1.0 / 2000.0; //FIXME: parameterize
		double allocsum = 0.0;
		for(int i=0;i<xvals.size();i++){
			assert(xvals[i] >= 0);
			allocsum += xvals[i];

			if(xvals[i] < minimalAllocation){
				DPRINTF(MissBWPolicy, "CPU %d got allocation %f, not enforcing for this CPU\n", i, xvals[i]);
				xvals[i] = -1.0;
			}
		}

		double threshold = 0.95; //FIXME: parameterize
		if(allocsum < (measurements->maxRequestRate * threshold)){
			DPRINTF(MissBWPolicy, "Sum of allocations %f is less than %f of maximum %f (%f), no allocations needed\n",
					allocsum,
					threshold,
					measurements->maxRequestRate,
					measurements->maxRequestRate * threshold);
			for(int i=0;i<xvals.size();i++) xvals[i] = -1.0;
		}

		traceVector("Final allocation is: ", xvals);
	}

	return xvals;
}

void
ModelThrottlingPolicy::implementAllocation(std::vector<double> allocation, double writeRate){

	if(implStrat == BW_IMPL_THROTTLE){
		vector<double> cyclesPerReq = vector<double>(allocation.size(), 0.0);
		for(int i=0;i<allocation.size();i++){
			cyclesPerReq[i] = 1.0 / allocation[i];
		}

		traceVector("Optimal cycles per request: ", cyclesPerReq);
		traceThrottling(allocation, cyclesPerReq, -1.0);

		traceVector("Setting memory bus arrival rates to:", allocation);
		sharedCacheThrottle->setTargetArrivalRate(allocation);
	}
	else if(implStrat == BW_IMPL_NFQ || implStrat == BW_IMPL_FIXED_BW){
		for(int i=0;i<buses.size();i++){
			buses[i]->setBandwidthQuotas(optimalBWShares);
		}

		traceThrottling(allocation, vector<double>(), writeRate);
	}
	else{
		fatal("Unknown bandwidth allocation implementation strategy");
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
ModelThrottlingPolicy::initModelValueTrace(int np){
	vector<string> headers;
	for(int i=0;i<cpuCount;i++) headers.push_back(RequestTrace::buildTraceName("alpha", i));
	for(int i=0;i<cpuCount;i++) headers.push_back(RequestTrace::buildTraceName("beta", i));
	for(int i=0;i<cpuCount;i++) headers.push_back(RequestTrace::buildTraceName("alone", i));
	for(int i=0;i<cpuCount;i++) headers.push_back(RequestTrace::buildTraceName("optimal-req-rate", i));
	headers.push_back("optimal-metric-value");
	modelValueTrace.initalizeTrace(headers);
}

void
ModelThrottlingPolicy::traceModelValues(PerformanceMeasurement* measurements, vector<double> optimalRates){
	vector<RequestTraceEntry> vals;
	for(int i=0;i<cpuCount;i++) vals.push_back(measurements->alphas[i]);
	for(int i=0;i<cpuCount;i++) vals.push_back(measurements->betas[i]);
	for(int i=0;i<cpuCount;i++) vals.push_back(aloneCycles[i]);
	for(int i=0;i<cpuCount;i++) vals.push_back(optimalRates[i]);
	vals.push_back(performanceMetric->computeFunction(measurements, optimalRates, aloneCycles));
	modelValueTrace.addTrace(vals);
}

void
ModelThrottlingPolicy::initThrottleTrace(int np){
	vector<string> headers;
	if(implStrat == BW_IMPL_THROTTLE){
		for(int i=0;i<cpuCount;i++) headers.push_back(RequestTrace::buildTraceName("Cycles between req CPU", i));
		for(int i=0;i<cpuCount;i++) headers.push_back(RequestTrace::buildTraceName("Arrival Rate CPU", i));
	}
	else{
		for(int i=0;i<cpuCount;i++) headers.push_back(RequestTrace::buildTraceName("Arrival Rate CPU", i));
		headers.push_back("Write Rate");
	}
	throttleTrace.initalizeTrace(headers);
}

void
ModelThrottlingPolicy::traceThrottling(std::vector<double> allocation, std::vector<double> throttles, double writeRate){
	vector<RequestTraceEntry> vals;
	for(int i=0;i<throttles.size();i++) vals.push_back(throttles[i]);
	for(int i=0;i<allocation.size();i++) vals.push_back(allocation[i]);
	if(writeRate != -1.0) vals.push_back(writeRate);
	throttleTrace.addTrace(vals);
}

double
ModelThrottlingPolicy::getTotalMisses(int cpuid, PerformanceMeasurement* measurements){
	return measurements->perCoreCacheMeasurements[cpuid].readMisses;
}

void
ModelThrottlingPolicy::dumpVerificationData(PerformanceMeasurement* measurements, std::vector<double> optimalArrivalRates){

	DataDump dumpvals = DataDump("throttling-data-dump.txt");
	if(cpuCount > 1){
		for(int i=0;i<cpuCount;i++) dumpvals.addElement(DataDump::buildKey("optimal-arrival-rates", i), optimalArrivalRates[i]);
		//for(int i=0;i<cpuCount;i++) dumpvals.addElement(DataDump::buildKey("optimal-periods", i), (int) optimalPeriods[i]);
		for(int i=0;i<cpuCount;i++) dumpvals.addElement(DataDump::buildKey("optimal-throttles", i), 1.0 / optimalArrivalRates[i]);
		for(int i=0;i<cpuCount;i++) dumpvals.addElement(DataDump::buildKey("optimal-bw-shares", i), optimalBWShares[i]);
		for(int i=0;i<cpuCount;i++) dumpvals.addElement(DataDump::buildKey("alone-cycles", i), RequestTraceEntry(aloneCycles[i]));
		for(int i=0;i<cpuCount;i++) dumpvals.addElement(DataDump::buildKey("alpha", i), measurements->alphas[i]);
		for(int i=0;i<cpuCount;i++) dumpvals.addElement(DataDump::buildKey("beta", i), measurements->betas[i]);
		for(int i=0;i<cpuCount;i++) dumpvals.addElement(DataDump::buildKey("cache-misses", i), measurements->perCoreCacheMeasurements[i].readMisses);
		for(int i=0;i<cpuCount;i++) dumpvals.addElement(DataDump::buildKey("alone-req-rates", i), (double) measurements->perCoreCacheMeasurements[i].readMisses / aloneCycles[i]);

		vector<double> measuredReqRates = vector<double>(cpuCount, 0.0);
		for(int i=0;i<cpuCount;i++){
			measuredReqRates[i] = getTotalMisses(i, measurements) / (double) curTick;
			dumpvals.addElement(DataDump::buildKey("measured-request-rate", i), measuredReqRates[i]);
		}

		dumpvals.addElement("cur-metric-value", performanceMetric->computeFunction(measurements, measuredReqRates, aloneCycles));
		dumpvals.addElement("opt-metric-value", performanceMetric->computeFunction(measurements, optimalArrivalRates, aloneCycles));
		dumpvals.addElement("max-bus-request-rate", measurements->maxRequestRate);
		dumpvals.addElement("uncontrollable-request-rate", measurements->getUncontrollableMissReqRate());
		dumpvals.addElement("uncontrollable-request-share", optimalBWShares[cpuCount]);
	}


	for(int i=0;i<cpuCount;i++) dumpvals.addElement(DataDump::buildKey("committed-instructions", i), measurements->committedInstructions[i]);
	for(int i=0;i<cpuCount;i++) dumpvals.addElement(DataDump::buildKey("requests", i), measurements->requestsInSample[i]);
	for(int i=0;i<cpuCount;i++) dumpvals.addElement(DataDump::buildKey("ticks", i), curTick);
	for(int i=0;i<cpuCount;i++) dumpvals.addElement(DataDump::buildKey("avg-bus-queue", i), measurements->latencyBreakdown[i][InterferenceManager::MemoryBusQueue]);
	for(int i=0;i<cpuCount;i++) dumpvals.addElement(DataDump::buildKey("avg-bus-service", i), measurements->latencyBreakdown[i][InterferenceManager::MemoryBusService]);
	for(int i=0;i<cpuCount;i++) dumpvals.addElement(DataDump::buildKey("avg-bus-entry", i), measurements->latencyBreakdown[i][InterferenceManager::MemoryBusEntry]);

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
	SimObjectParam<ThrottleControl* > sharedCacheThrottle;
	SimObjectVectorParam<ThrottleControl* > privateCacheThrottles;
	Param<bool> verify;
	VectorParam<double> staticArrivalRates;
	Param<string> implStrategy;
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
	INIT_PARAM(sharedCacheThrottle, "Shared cache throttle"),
	INIT_PARAM(privateCacheThrottles, "Private cache throttles"),
	INIT_PARAM_DFLT(verify, "Verify policy", false),
	INIT_PARAM_DFLT(staticArrivalRates, "Static arrival rates to enforce", vector<double>()),
	INIT_PARAM_DFLT(implStrategy, "The way to enforce the bandwidth quotas", "throttle")
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
							         sharedCacheThrottle,
							         privateCacheThrottles,
							         verify,
							         staticArrivalRates,
							         implStrategy);
}

REGISTER_SIM_OBJECT("ModelThrottlingPolicy", ModelThrottlingPolicy)

#endif //DOXYGEN_SHOULD_SKIP_THIS


