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

#ifndef __BASE_STATS_MYSQL_RUN_HH__
#define __BASE_STATS_MYSQL_RUN_HH__

#include <string>

#include "base/mysql.hh"
#include "sim/host.hh"

namespace Stats {

struct MySqlRun
{
  private:
    MySQL::Connection mysql;
    uint16_t run_id;

  protected:
    void setup(const std::string &name, const std::string &sample,
	       const std::string &user, const std::string &project);

    void remove(const std::string &name);
    void cleanup();

  public:
    bool connected() const { return mysql.connected(); }
    void connect(const std::string &host, const std::string &user,
		 const std::string &passwd, const std::string &db,
		 const std::string &name, const std::string &sample,
		 const std::string &project);

    MySQL::Connection &conn() { return mysql; }
    uint16_t run() const { return run_id; }
};

/* namespace Stats */ }

#endif // __BASE_STATS_MYSQL_RUN_HH__
