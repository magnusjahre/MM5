/*
 * Copyright (c) 2005
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

#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <fstream>

#include "base/misc.hh"
#include "base/output.hh"

using namespace std;

OutputDirectory simout;

/**
 *
 */
OutputDirectory::OutputDirectory()
{}

OutputDirectory::~OutputDirectory()
{}

void
OutputDirectory::setDirectory(const string &d)
{
    if (!dir.empty())
	panic("Output directory already set!\n");

    dir = d;

    if (dir != ".") {
	if (mkdir(dir.c_str(), 0777) < 0 && errno != EEXIST)
	    panic("couldn't make output dir %s: %s\n",
		  dir, strerror(errno));
    }

    // guarantee that directory ends with a '/'
    if (dir[dir.size() - 1] != '/')
	dir += "/";
}

const string &
OutputDirectory::directory()
{
    if (dir.empty())
	panic("Output directory not set!");

    return dir;
}

string
OutputDirectory::resolve(const string &name)
{
    return (name[0] != '/') ? dir + name : name;
}

ostream *
OutputDirectory::create(const string &name)
{
    if (name == "cerr" || name == "stderr")
	return &cerr;

    if (name == "cout" || name == "stdout")
	return &cout;

    ofstream *file = new ofstream(resolve(name).c_str(), ios::trunc);
    if (!file->is_open())
	panic("Cannot open file %s", name);

    return file;
}

ostream *
OutputDirectory::find(const string &name)
{
    if (name == "cerr" || name == "stderr")
	return &cerr;

    if (name == "cout" || name == "stdout")
	return &cout;

    string filename = resolve(name);
    map_t::iterator i = files.find(filename);
    if (i != files.end())
	return (*i).second;

    ofstream *file = new ofstream(filename.c_str(), ios::trunc);
    if (!file->is_open())
	panic("Cannot open file %s", filename);

    files[filename] = file;
    return file;
}

bool
OutputDirectory::isFile(const std::ostream *os)
{
    return os && os != &cerr && os != &cout;
}
