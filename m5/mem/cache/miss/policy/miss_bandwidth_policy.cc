/*
 * miss_bandwidth_policy.cc
 *
 *  Created on: Oct 28, 2009
 *      Author: jahre
 */

#include "miss_bandwidth_policy.hh"

using namespace std;

MissBandwidthPolicy::MissBandwidthPolicy(string _name, InterferenceManager* _intManager, Tick _period):
SimObject(_name){

	intManager = _intManager;

	period = _period;
	policyEvent = new MissBandwidthPolicyEvent(this, period);
	policyEvent->schedule(period);
}

MissBandwidthPolicy::~MissBandwidthPolicy(){
	assert(!policyEvent->scheduled());
	delete policyEvent;
}

void
MissBandwidthPolicy::handlePolicyEvent(){
	intManager->buildInterferenceMeasurement();
}

