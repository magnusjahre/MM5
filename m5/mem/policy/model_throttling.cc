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
#define SHARED_CACHE_WAYS 16

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
			   	    			 std::vector<double> _staticArrivalRates,
			   	    			 std::string _implStrategy,
			   	    			 WriteStallTechnique _wst,
			   	    			 PrivBlockedStallTechnique _pbst,
			   	    			 EmptyROBStallTechnique _rst,
								 double _maximumDamping,
								 double _hybridDecisionError,
								 int _hybridBufferSize)
: BasePolicy(_name, _intManager, _period, _cpuCount, _perfEstMethod, _persistentAllocations, _iterationLatency, _performanceMetric, _enforcePolicy, _wst, _pbst, _rst, _maximumDamping, _hybridDecisionError, _hybridBufferSize)
{
	//enableOccupancyTrace = true;

	doVerification = _verify;

	optimalPeriods.resize(_cpuCount, 0.0);
	optimalBWShares = vector<double>(cpuCount+1, 0.0);
	optimalWayAllocs = vector<int>(cpuCount, 0);

	predictedCPI.resize(_cpuCount, 0.0);

	modelValueTrace = RequestTrace(_name, "ModelValueTrace");
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

	throttleTrace = RequestTrace(_name, "ThrottleTrace");
	initThrottleTrace(_cpuCount);

	searchItemNum = 0;
	if(doVerification){
		modelSearchTrace = RequestTrace(_name, "ModelSearchTrace");
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

	if ((lp = make_lp(0,cpuCount*2)) == NULL) fatal("Couldn't create LP");
	set_outputfile(lp, (char*) "lp-trace.txt");

	REAL* rowbuffer = (REAL*) malloc((cpuCount*2+1)*sizeof(REAL));
	for(int i=0;i<=cpuCount*2;i++) rowbuffer[i] = 0.0;

	// sum of bandwidth shares must be less than 1.0
	for(int i=1;i<=cpuCount;i++) rowbuffer[i] = 1.0;
	if (!add_constraint(lp, rowbuffer, LE, 1.0)) fatal("Couldn't add sum constraint");

	// ad starvation constraints and upper bounds
	for(int i=1;i<=cpuCount;i++) rowbuffer[i] = 0.0;
	for(int i=1;i<=cpuCount;i++){

		double thisMaxBW = (double) measurements->perCoreCacheMeasurements[i-1].readMisses / (double) aloneCycles[i-1];
		double upbo = thisMaxBW / measurements->maxReadRequestRate;

		double lowbo = 0.05;
		if(lowbo > upbo){
			upbo = lowbo;
		}

		// starvation constraint
		rowbuffer[i-1] = 0.0;
		rowbuffer[i] = 1.0;
		if (!add_constraint(lp, rowbuffer, GE, lowbo)) fatal("Couldn't add starvation constraint");

		// upper bound
		set_upbo(lp, i, upbo);
	}

	// set cache sum constraint
	for(int i=0;i<=cpuCount*2;i++) rowbuffer[i] = 0.0;
	for(int i=cpuCount+1;i<=cpuCount*2;i++){
		rowbuffer[i] = 1.0;
	}
	if (!add_constraint(lp, rowbuffer, LE, SHARED_CACHE_WAYS)) fatal("Couldn't add cache sum constraint");

	for(int i=cpuCount+1;i<=cpuCount*2;i++){
		set_lowbo(lp, i , 1.0);
	}

	// set objective function
    for(int i=1;i<=cpuCount*2;i++){
    	rowbuffer[i] = gradient[i-1];
    }
    set_obj_fn(lp, rowbuffer);

    set_maxim(lp);

    print_lp(lp);

    int ret = solve(lp);
    if(ret != OPTIMAL){
    	warn("Linear programming solver returned non-optimal result (Code %d)", ret);
    	return vector<double>();
    }

    vector<double> retval = vector<double>(cpuCount*2, 0.0);

    // retrieve bandwidth allocations
    for(int i=0;i<cpuCount;i++){
    	retval[i] = get_var_primalresult(lp, i+2);
    }

    // retrieve cache allocations
    for(int i=0;i<cpuCount;i++){
    	retval[i+cpuCount] = get_var_primalresult(lp, 3+i+cpuCount*2);
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

	vector<double> xvals = vector<double>(cpuCount*2, 1.0 / (double) cpuCount);
	for(int i=cpuCount;i<2*cpuCount;i++) xvals[i] = (double) ((double) SHARED_CACHE_WAYS / (double) cpuCount);

	vector<double> fairShare = xvals;

	vector<double> xstar = vector<double>(cpuCount*2, 0.0);

	traceVector("Initial xvector is: ", xvals);

	vector<double> thisGradient = performanceMetric->gradient(measurements, aloneCycles, cpuCount, xvals);

	traceVector("Initial gradient is: ", thisGradient);

	double gradsum = 0;
	for(int i=0;i<thisGradient.size();i++) gradsum += thisGradient[i];
	if(gradsum == 0){
		DPRINTF(MissBWPolicy, "All gradient values are zero, fair share allocation\n");
		return fairShare;
	}


	if(doVerification) traceSearch(xvals);
	int cutoff = 0;
	bool quitForCutoff = false;

	while(checkConvergence(xstar, xvals, thisGradient) && !quitForCutoff){
		thisGradient = performanceMetric->gradient(measurements, aloneCycles, cpuCount, xvals);
		traceVerboseVector("New gradient is: ", thisGradient);
		xstar = findNewTrialPoint(thisGradient, measurements);
		if(xstar.empty()){
			warn("Linear programming problem not solved optimally, returning initial solution\n");
			xvals = vector<double>(cpuCount, 1.0 / (double) cpuCount);
			break;
		}

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

	traceVector("Optimal solution vector is: ", xvals);

	double maxbw = measurements->maxReadRequestRate + measurements->getUncontrollableMissReqRate();
	for(int i=0;i<cpuCount;i++) optimalBWShares[i] = (xvals[i] * measurements->maxReadRequestRate)  / maxbw;
	optimalBWShares[cpuCount] = measurements->getUncontrollableMissReqRate() / maxbw;
	traceVector("Optimal bandwidth shares are: ", optimalBWShares);

	vector<double> optimalReqRates = vector<double>(cpuCount+1, 0.0);
	for(int i=0;i<=cpuCount;i++) optimalReqRates[i] = optimalBWShares[i] * maxbw;

	traceVector("Optimal request rates are: ", optimalReqRates);

	for(int i=0;i<cpuCount;i++){
		optimalWayAllocs[i] = (int) xvals[i+cpuCount];
	}
	fixCacheAllocations();

	traceVector("Optimal cache partitions are: ", optimalWayAllocs);

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
		if(allocsum < (measurements->maxReadRequestRate * threshold)){
			DPRINTF(MissBWPolicy, "Sum of allocations %f is less than %f of maximum %f (%f), no allocations needed\n",
					allocsum,
					threshold,
					measurements->maxReadRequestRate,
					measurements->maxReadRequestRate * threshold);
			for(int i=0;i<xvals.size();i++) xvals[i] = -1.0;
		}

		traceVector("Final allocation is: ", xvals);
	}

	return xvals;
}

void
ModelThrottlingPolicy::fixCacheAllocations(){
	int waysum = 0;
	for(int i=0;i<optimalWayAllocs.size();i++) waysum += optimalWayAllocs[i];

	int iterations = 0;
	while(waysum < SHARED_CACHE_WAYS){
		optimalWayAllocs[iterations%cpuCount] += 1;
		waysum +=1;
		iterations+=1;
	}
	traceVector("Corrected way allocation is: ", optimalWayAllocs);
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
		fatal("Shared cache throttle have been moved to MissBandwidthPolicy");
		//sharedCacheThrottle->setTargetArrivalRate(allocation);
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

	int allocsum = 0;
	for(int i=0;i<optimalWayAllocs.size();i++) allocsum += optimalWayAllocs[i];

	if(allocsum > 0){
		for(int i=0;i<sharedCaches.size();i++){
			traceVector("Implementing optimal cache partition:", optimalWayAllocs);
			sharedCaches[i]->setCachePartition(optimalWayAllocs);
			sharedCaches[i]->enablePartitioning();
		}
	}
	else{
		DPRINTF(MissBWPolicy, "No ways allocated, skipping way allocation \n");
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
	for(int i=0;i<cpuCount;i++) headers.push_back(RequestTrace::buildTraceName("CPI estimate", i));
	for(int i=0;i<cpuCount;i++) headers.push_back(RequestTrace::buildTraceName("CPI actual", i));
	for(int i=0;i<cpuCount;i++) headers.push_back(RequestTrace::buildTraceName("Optimal BW Shares", i));
	headers.push_back("optimal-metric-value");
	modelValueTrace.initalizeTrace(headers);
}

void
ModelThrottlingPolicy::traceModelValues(PerformanceMeasurement* measurements, vector<double> optimalRates){
	vector<RequestTraceEntry> vals;
	for(int i=0;i<cpuCount;i++) vals.push_back(measurements->alphas[i]);
	for(int i=0;i<cpuCount;i++) vals.push_back(measurements->betas[i]);
	for(int i=0;i<cpuCount;i++) vals.push_back(aloneCycles[i]);
	for(int i=0;i<cpuCount;i++) vals.push_back(predictedCPI[i]);
	for(int i=0;i<cpuCount;i++) vals.push_back((double) period /(double) measurements->committedInstructions[i]);

	vector<double> procshares;
	for(int i=0;i<cpuCount;i++){
		vals.push_back(optimalBWShares[i]);
		procshares.push_back(optimalBWShares[i]);
	}

	for(int i=0;i<cpuCount;i++){
		procshares.push_back(optimalWayAllocs[i]);
	}

	vals.push_back(performanceMetric->computeFunction(measurements, procshares, aloneCycles));
	modelValueTrace.addTrace(vals);

	for(int i=0;i<cpuCount;i++){
		predictedCPI[i] = (((measurements->alphas[i] * measurements->getMisses(i, optimalWayAllocs[i]) ) / optimalBWShares[i]) + measurements->betas[i]) / (double) measurements->committedInstructions[i];
	}
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
		dumpvals.addElement("max-bus-request-rate", measurements->maxReadRequestRate);
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
	Param<bool> verify;
	VectorParam<double> staticArrivalRates;
	Param<string> implStrategy;
	Param<string> writeStallTechnique;
	Param<string> privateBlockedStallTechnique;
	Param<string> emptyROBStallTechnique;
	Param<double> maximumDamping;
    Param<double> hybridDecisionError;
    Param<int> hybridBufferSize;
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
	INIT_PARAM_DFLT(staticArrivalRates, "Static arrival rates to enforce", vector<double>()),
	INIT_PARAM_DFLT(implStrategy, "The way to enforce the bandwidth quotas", "throttle"),
	INIT_PARAM(writeStallTechnique, "The technique to use to estimate private write stalls"),
	INIT_PARAM(privateBlockedStallTechnique, "The technique to use to estimate private blocked stalls"),
	INIT_PARAM(emptyROBStallTechnique, "The technique to use to estimate private mode empty ROB stalls"),
	INIT_PARAM_DFLT(maximumDamping, "The maximum absolute damping the damping policies can apply", 0.25),
    INIT_PARAM_DFLT(hybridDecisionError, "The error at which to switch from CPL to CPL-CWP with the hybrid scheme", 0.0),
    INIT_PARAM_DFLT(hybridBufferSize, "The number of errors to use in the decision buffer", 3)
END_INIT_SIM_OBJECT_PARAMS(ModelThrottlingPolicy)

CREATE_SIM_OBJECT(ModelThrottlingPolicy)
{

	BasePolicy::PerformanceEstimationMethod perfEstMethod =
		BasePolicy::parsePerformanceMethod(performanceEstimationMethod);

	Metric* performanceMetric = BasePolicy::parseOptimizationMetric(optimizationMetric);

	BasePolicy::WriteStallTechnique wst = BasePolicy::parseWriteStallTech(writeStallTechnique);
	BasePolicy::PrivBlockedStallTechnique pbst = BasePolicy::parsePrivBlockedStallTech(privateBlockedStallTechnique);
	BasePolicy::EmptyROBStallTechnique rst = BasePolicy::parseEmptyROBStallTech(emptyROBStallTechnique);

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
							         staticArrivalRates,
							         implStrategy,
							         wst,
							         pbst,
							         rst,
							         maximumDamping,
							         hybridDecisionError,
							         hybridBufferSize);
}

REGISTER_SIM_OBJECT("ModelThrottlingPolicy", ModelThrottlingPolicy)

#endif //DOXYGEN_SHOULD_SKIP_THIS


