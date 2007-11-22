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

#include <iostream>

#include "base/mysql.hh"
#include "base/trace.hh"

using namespace std;

namespace MySQL {

inline const char *
charstar(const string &string)
{
    return string.empty() ? NULL : string.c_str();
}

ostream &
operator<<(ostream &stream, const Error &error)
{
    stream << error.string();
    return stream;
}

/*
 * The connection class
 */
Connection::Connection()
    : valid(false)
{
}

Connection::~Connection()
{
    if (valid)
	close();
}


bool
Connection::connect(const string &xhost, const string &xuser,
		    const string &xpasswd, const string &xdatabase)
{
    if (connected())
	return error.set("Already Connected");

    _host = xhost;
    _user = xuser;
    _passwd = xpasswd;
    _database = xdatabase;

    error.clear();

    mysql_init(&mysql);
    mysql_options(&mysql, MYSQL_OPT_COMPRESS, 0); // might want to be 1
    mysql_options(&mysql, MYSQL_READ_DEFAULT_GROUP, "odbc");
    if (!mysql_real_connect(&mysql, charstar(_host), charstar(_user),
			    charstar(_passwd), charstar(_database),
			    0, NULL, 0))
	return error.set(mysql_error(&mysql));

    valid = true;
    return false;
}

void
Connection::close()
{
    mysql_close(&mysql);
}

bool
Connection::query(const string &sql)
{
    DPRINTF(SQL, "Sending SQL query to server:\n%s", sql);
    error.clear();
    if (mysql_real_query(&mysql, sql.c_str(), sql.size()))
	error.set(mysql_error(&mysql));

    return error;
}


/* namespace MySQL */ }
