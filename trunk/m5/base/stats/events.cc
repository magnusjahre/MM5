/*
 * Copyright (c) 2004, 2005
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

#include <vector>

#include "base/stats/events.hh"

#if USE_MYSQL
#include "base/cprintf.hh"
#include "base/misc.hh"
#include "base/mysql.hh"
#include "base/stats/mysql.hh"
#include "base/stats/mysql_run.hh"
#include "base/str.hh"
#endif

#include "base/match.hh"
#include "sim/host.hh"
#include "sim/sim_object.hh"
#include "sim/root.hh"

using namespace std;

namespace Stats {

Tick EventStart = ULL(0x7fffffffffffffff);

ObjectMatch event_ignore;

#if USE_MYSQL
class InsertEvent
{
  private:
    char *query;
    int size;
    bool first;
    static const int maxsize = 1024*1024;

    typedef map<string, uint32_t> event_map_t;
    event_map_t events;

    MySQL::Connection &mysql;
    uint16_t run;

  public:
    InsertEvent()
	: mysql(MySqlDB.conn()), run(MySqlDB.run())
    {
	query = new char[maxsize + 1];
	size = 0;
	first = true;
	flush();
    }
    ~InsertEvent()
    {
	flush();
    }

    void flush();
    void insert(const string &stat);
};

void
InsertEvent::insert(const string &stat)
{
    assert(mysql.connected());

    event_map_t::iterator i = events.find(stat);
    uint32_t event;
    if (i == events.end()) {
	mysql.query(
	    csprintf("SELECT en_id "
		     "from event_names "
		     "where en_name=\"%s\"",
		     stat));

	MySQL::Result result = mysql.store_result();
	if (!result)
	    panic("could not get a run\n%s\n", mysql.error);

	assert(result.num_fields() == 1);
	MySQL::Row row = result.fetch_row();
	if (row) {
	    if (!to_number(row[0], event))
		panic("invalid event id: %s\n", row[0]);
	} else {
	    mysql.query(
		csprintf("INSERT INTO "
			 "event_names(en_name)"
			 "values(\"%s\")",
			 stat));

	    if (mysql.error)
		panic("could not get a run\n%s\n", mysql.error);

	    event = mysql.insert_id();
	}
    } else {
	event = (*i).second;
    }

    if (size + 1024 > maxsize)
	flush();

    if (!first) {
	query[size++] = ',';
	query[size] = '\0';
    }

    first = false;

    size += sprintf(query + size, "(%u,%u,%llu)",
		    event, run, (unsigned long long)curTick);
}

void
InsertEvent::flush()
{
    static const char query_header[] = "INSERT INTO "
	"events(ev_event, ev_run, ev_tick)"
	"values";

    if (size) {
	MySQL::Connection &mysql = MySqlDB.conn();
	assert(mysql.connected());
	mysql.query(query);
    }

    query[0] = '\0';
    size = sizeof(query_header);
    first = true;
    memcpy(query, query_header, size);
}

void
__event(const string &stat)
{
    static InsertEvent event;
    event.insert(stat);
}

#endif

/* namespace Stats */ }
