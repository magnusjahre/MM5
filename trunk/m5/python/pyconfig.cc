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

#include <sys/wait.h>
#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <unistd.h>

#include <cstdlib>
#include <cstdio>
#include <sstream>
#include <string>

#include "base/embedfile.hh"
#include "base/inifile.hh"
#include "base/misc.hh"
#include "base/output.hh"
#include "python/pyconfig.hh"

using namespace std;

PythonConfig::PythonConfig()
{
    // Add directory where m5 was invoked to Python path.
    char path[FILENAME_MAX];
    getcwd(path, sizeof(path));
    addPath(path);
}

PythonConfig::~PythonConfig()
{
}

void
PythonConfig::writeLine(const string &s)
{
    ccprintf(data, "%s\n", s);
}

void
PythonConfig::setVariable(const string &var, const string &val)
{
    ccprintf(data, "%s = '%s'\n", var, val);
}

void
PythonConfig::addPath(const string &path)
{
    ccprintf(data, "AddToPath('%s')\n", path);
}

void
PythonConfig::load(const string &filename)
{
    ccprintf(data, "m5execfile('%s', globals())\n", filename);
}

bool
PythonConfig::output(IniFile &iniFile)
{
    string pyfilename = simout.resolve("config.py");
    string inifilename = simout.resolve("config.ini");

    ofstream pyfile(pyfilename.c_str());

    EmbedFile embedded;
    if (!EmbedMap::get("string_importer.py", embedded))
	panic("string_importer.py is not embedded in the m5 binary");
    pyfile.write(embedded.data, embedded.length);

    if (!EmbedMap::get("embedded_py.py", embedded))
	panic("embedded_py.py is not embedded in the m5 binary");
    pyfile.write(embedded.data, embedded.length);

    if (!EmbedMap::get("defines.py", embedded))
	panic("defines.py is not embedded in the m5 binary");
    pyfile.write(embedded.data, embedded.length);

    ccprintf(pyfile, "from m5 import *\n");

    pyfile << data.str();

    ccprintf(pyfile,
	     "if globals().has_key('root') and isinstance(root, Root):\n"
	     "    instantiate(root)\n"
	     "else:\n"
	     "    print 'Instantiation skipped: no root object found.'\n");

    pyfile.close();

    string script = csprintf("python %s > %s", pyfilename, inifilename);
    int ret = system(script.c_str());
    if (ret == -1)
	panic("system(\"python config.py > config.ini\"): %s",
	      strerror(errno));

    if (ret != 0)
	exit(1);

    return iniFile.load(inifilename);
}
