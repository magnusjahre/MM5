/*
 * Copyright (c) 2002, 2003, 2004, 2005
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
#include <fstream>
#include <string>
#include <vector>

#include "base/inifile.hh"
#include "base/cprintf.hh"

using namespace std;

char *progname;

void
usage()
{
    cout << "Usage: " << progname << " <ini file>\n";
    exit(1);
}

#if 0
char *defines = getenv("CONFIG_DEFINES");
if (defines) {
    char *c = defines;
    while ((c = strchr(c, ' ')) != NULL) {
	*c++ = '\0';
	count++;
    }
    count++;
}

#endif

int
main(int argc, char *argv[])
{
    IniFile simConfigDB;

    progname = argv[0];

    vector<char *> cppArgs;

    vector<char *> cpp_options;
    cpp_options.reserve(argc * 2);

    for (int i = 1; i < argc; ++i) {
	char *arg_str = argv[i];

	// if arg starts with '-', parse as option,
	// else treat it as a configuration file name and load it
	if (arg_str[0] == '-') {

	    // switch on second char
	    switch (arg_str[1]) {
	      case 'D':
	      case 'U':
	      case 'I':
		// cpp options: record & pass to cpp.  Note that these
		// cannot have spaces, i.e., '-Dname=val' is OK, but
		// '-D name=val' is not.  I don't consider this a
		// problem, since even though gnu cpp accepts the
		// latter, other cpp implementations do not (Tru64,
		// for one).
		cppArgs.push_back(arg_str);
		break;

	      case '-':
		// command-line configuration parameter:
		// '--<section>:<parameter>=<value>'

		if (!simConfigDB.add(arg_str + 2)) {
		    // parse error
		    ccprintf(cerr,
			     "Could not parse configuration argument '%s'\n"
			     "Expecting --<section>:<parameter>=<value>\n",
			     arg_str);
		    exit(0);
		}
		break;

	      default:
		usage();
	    }
	}
	else {
	    // no '-', treat as config file name

	    if (!simConfigDB.loadCPP(arg_str, cppArgs)) {
		cprintf("Error processing file %s\n", arg_str);
		exit(1);
	    }
	}
    }

    string value;

#define FIND(C, E) \
  if (simConfigDB.find(C, E, value)) \
    cout << ">" << value << "<\n"; \
  else \
    cout << "Not Found!\n"

    FIND("General", "Test2");
    FIND("Junk", "Test3");
    FIND("Junk", "Test4");
    FIND("General", "Test1");
    FIND("Junk2", "test3");
    FIND("General", "Test3");

    cout << "\n";

    simConfigDB.dump();

    return 0;
}
