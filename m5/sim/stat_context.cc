/*
 * Copyright (c) 2003, 2004, 2005
 * The Regents of The University of Michigan
 * All Rights Reserved
 *
 * This code is part of the M5 simulator, developed by Nathan Binkert,
 * Erik Hallnor, Steve Raasch, and Steve Reinhardt, with contributions
 * from Ron Dreslinski, Dave Greene, Lisa Hsu, Kevin Lim, Ali Saidi,
 * and Andrew Schultz.
 *
 * Permission is granted to use, copy, create derivative works and
 * redistribute this software and such derivative works for any
 * purpose, so long as the copyright notice above, this grant of
 * permission, and the disclaimer below appear in all copies made; and
 * so long as the name of The University of Michigan is not used in
 * any advertising or publicity pertaining to the use or distribution
 * of this software without specific, written prior authorization.
 *
 * THIS SOFTWARE IS PROVIDED AS IS, WITHOUT REPRESENTATION FROM THE
 * UNIVERSITY OF MICHIGAN AS TO ITS FITNESS FOR ANY PURPOSE, AND
 * WITHOUT WARRANTY BY THE UNIVERSITY OF MICHIGAN OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE. THE REGENTS OF THE UNIVERSITY OF MICHIGAN SHALL NOT BE
 * LIABLE FOR ANY DAMAGES, INCLUDING DIRECT, SPECIAL, INDIRECT,
 * INCIDENTAL, OR CONSEQUENTIAL DAMAGES, WITH RESPECT TO ANY CLAIM
 * ARISING OUT OF OR IN CONNECTION WITH THE USE OF THE SOFTWARE, EVEN
 * IF IT HAS BEEN OR IS HEREAFTER ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGES.
 */

// This file will contain default statistics for the simulator that
// don't really belong to a specific simulator object

#include <iostream>
#include <string>

#include "base/callback.hh"
#include "base/hostinfo.hh"
#include "base/match.hh"
#include "base/output.hh"
#include "base/statistics.hh"
#include "base/time.hh"
#include "base/userinfo.hh"
#include "base/stats/events.hh"
#if USE_MYSQL
#include "base/stats/mysql.hh"
#include "base/stats/mysql_run.hh"
#endif
#include "base/stats/text.hh"
#include "sim/param.hh"
#include "sim/sim_object.hh"
#include "sim/stats.hh"
#include "sim/stat_control.hh"
#include "sim/root.hh"

using namespace std;

Stats::Text text;
#if USE_MYSQL
Stats::MySql mysql;
#endif

namespace Stats {
extern ObjectMatch event_ignore;
/* namespace Stats */ }

namespace {
class StatsParamContext : public ParamContext
{
  public:
    StatsParamContext(const string &_iniSection);
    ~StatsParamContext();
    void checkParams();
    void startup();
};

StatsParamContext statsParams("stats");
Param<bool> stat_print_desc(&statsParams, "descriptions",
			    "display statistics descriptions", true);
Param<string> stat_project_name(&statsParams, "project_name",
				  "project name for statistics comparison",
				   "test");
Param<string> stat_simulation_name(&statsParams, "simulation_name",
				  "simulation name for statistics comparison",
				   "test");
Param<string> stat_simulation_sample(&statsParams, "simulation_sample",
				     "sample for statistics aggregation",
				     "0");

Param<string> stat_print_file(&statsParams, "text_file",
			      "file to dump stats to");

Param<bool> stat_print_compat(&statsParams, "text_compat",
			      "simplescalar stats compatibility", true);

Param<string> stat_mysql_database(&statsParams, "mysql_db",
			    "mysql database to put data into", "");

Param<string> stat_mysql_user(&statsParams, "mysql_user",
			    "username for mysql", "");

Param<string> stat_mysql_password(&statsParams, "mysql_password",
			    "password for mysql user", "");

Param<string> stat_mysql_host(&statsParams, "mysql_host",
			    "host for mysql", "zizzer.eecs.umich.edu");

Param<Tick> stat_event_start(&statsParams, "events_start",
			     "cycle to start tracking events", (Tick)-1);

Param<bool> dump_reset(&statsParams, "dump_reset",
		       "when dumping stats, reset afterwards", false);

Param<Tick> dump_cycle(&statsParams, "dump_cycle",
		       "cycle on which to dump stats", 0);

Param<Tick> dump_period(&statsParams, "dump_period",
			"period with which to dump stats", 0);

VectorParam<string> ignore(&statsParams, "ignore_events",
			   "name strings to ignore",
			   vector<string>());

StatsParamContext::StatsParamContext(const string &_iniSection)
    : ParamContext(_iniSection, StatsInitPhase)
{}

StatsParamContext::~StatsParamContext()
{
}

void
StatsParamContext::checkParams()
{
    using namespace Stats;

    if (!((string)stat_print_file).empty()) {
    	text.open(*simout.find(stat_print_file));
    	text.descriptions = stat_print_desc;
    	text.compat = stat_print_compat;
    	OutputList.push_back(&text);
    }

#if USE_MYSQL
    if (!((string)stat_mysql_database).empty()) {
	string user = stat_mysql_user;
	if (user.empty())
	    user = username();

	MySqlDB.connect(stat_mysql_host,
			user,
			stat_mysql_password,
			stat_mysql_database,
			stat_simulation_name,
			stat_simulation_sample,
			stat_project_name);
	OutputList.push_back(&mysql);
    }
#endif

    event_ignore.setExpression(ignore);
}

void
StatsParamContext::startup()
{
    using namespace Stats;

    if (stat_event_start != (Tick)-1)
	EventStart = stat_event_start;

    if (dump_cycle > 0 || dump_period > 0) {
	// if dump_period is specified but not dump_cycle, then first dump
	// should be at dump_period cycles.
	Tick first = dump_cycle > 0 ? Tick(dump_cycle) : Tick(dump_period);
	Tick repeat = dump_period;

	int flags = Dump;
	if (dump_reset)
	    flags |= Reset;

	Stats::SetupEvent(flags, curTick + first, repeat);
    }
}

/* namespace */ }
