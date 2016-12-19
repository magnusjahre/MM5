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

class CacheMissMeasurements{
public:
	int readMisses;
	int wbMisses;
	int interferenceMisses;
	int accesses;
	int sharedCacheWritebacks;
	std::vector<int> privateCumulativeCacheMisses;

	CacheMissMeasurements()
	: readMisses(0), wbMisses(0), interferenceMisses(0), accesses(0), sharedCacheWritebacks(0){
	}

	CacheMissMeasurements(int _readMisses, int _wbMisses, int _interferenceMisses, int _accesses, int _scwb, std::vector<int> _privateCumulativeCacheMisses)
	: readMisses(_readMisses), wbMisses(_wbMisses), interferenceMisses(_interferenceMisses), accesses(_accesses), sharedCacheWritebacks(_scwb), privateCumulativeCacheMisses(_privateCumulativeCacheMisses){
	}

//	void add(CacheMissMeasurements newValues){
//		readMisses += newValues.readMisses;
//		wbMisses += newValues.wbMisses;
//		interferenceMisses += newValues.interferenceMisses;
//		accesses += newValues.accesses;
//		sharedCacheWritebacks += newValues.sharedCacheWritebacks;
//	}

	double getInterferenceMissesPerMiss();

	double getMissRate();

	void printMissCurve();
};

class PerformanceMeasurement{
private:
	int cpuCount;
	int numIntTypes;
	int maxMSHRs;
	int period;
	double uncontrollableMissRequestRate;

	class CacheMissModel{
	public:
		int a;
		int b;
		double gradient;

		CacheMissModel(){
			a = -1;
			b = -1;
			gradient = 0.0;
		}

		CacheMissModel(int _a, int _b, double _gradient)
		: a(_a), b(_b), gradient(_gradient){

		}

		void dump();
	};

	std::vector<CacheMissModel> cacheMissModels;

public:
	std::vector<int> committedInstructions;
	std::vector<int> requestsInSample;
	std::vector<int> responsesWhileStalled;

	std::vector<int> cpuSharedStallCycles;

	std::vector<std::vector<double> > mlpEstimate;
	std::vector<std::vector<double> > avgMissesWhileStalled;

	std::vector<double> totalSharedLatencies;
	std::vector<double> sharedLatencies;
	std::vector<double> estimatedPrivateLatencies;
	std::vector<std::vector<double> > latencyBreakdown;
	std::vector<std::vector<double> > privateLatencyBreakdown;

	std::vector<double> cacheInterferenceLatency;

	std::vector<double> alphas;
	std::vector<double> betas;
	std::vector<double> computedOverlap;
	std::vector<double> currentBWShare;

	double actualBusUtilization;
	double sharedCacheMissRate;

	std::vector<int> busAccessesPerCore;
	std::vector<int> busReadsPerCore;

	std::vector<int> latencySyncedLLCMisses;
	std::vector<double> sumMembusLatency;

	double avgBusServiceCycles;
	int otherBusRequests;
	double sumBusQueueCycles;

	std::vector<CacheMissMeasurements> perCoreCacheMeasurements;

	double maxReadRequestRate;
	double maxRate;

	PerformanceMeasurement(int _cpuCount, int _numIntTypes, int _maxMSHRs, int _period);

	void addInterferenceData(std::vector<double> sharedLatencyAccumulator,
						     std::vector<double> interferenceEstimateAccumulator,
							 std::vector<std::vector<double> > sharedLatencyBreakdownAccumulator,
							 std::vector<std::vector<double> > interferenceBreakdownAccumulator,
							 std::vector<int> localRequests,
							 std::vector<int> llcMissesSyncToLatencyAccumulator);

	std::vector<std::string> getTraceHeader();
	std::vector<RequestTraceEntry> createTraceLine();

	double getNonStallCycles(int cpuID, int period);

	int getPeriod(){
		return period;
	}

	void updateConstants();

	double getMisses(int cpuID, double inWays);

	double getMissGradient(int cpuID);

	bool inFlatSection(int cpuID, double inWays);

	double getUncontrollableMissReqRate(){
		return uncontrollableMissRequestRate;
	}

	std::vector<double> getSharedPreLLCAvgLatencies();

	std::vector<double> getSharedModeIPCs();

	std::vector<double> getSharedModeCPIs();

	double getGlobalAvgMemBusLatency();

private:
	void updateAlpha(int cpuID);

	void updateBeta(int cpuID);

	void setBandwidthBound();

	void calibrateMissModel();

	double ratio(int a, int b);
};


#endif /* PERFORMANCE_MEASUREMENT_HH_ */
