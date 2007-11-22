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

#ifndef __BASE_EMBEDFILE_HH__
#define __BASE_EMBEDFILE_HH__

#include <list>
#include <map>
#include <string>

struct EmbedFile
{
    std::string name;
    const char *data;
    int length;

    EmbedFile() {}
    EmbedFile(const std::string &n, const char *d, int l)
	: name(n), data(d), length(l)
    {}
    EmbedFile(const EmbedFile &f)
	: name(f.name), data(f.data), length(f.length)
    {}
    
    const EmbedFile &operator=(const EmbedFile &f)
    {
	if (&f != this) {
	    name = f.name;
	    data = f.data;
	    length = f.length;
	}
	return *this;
    }
};

class EmbedMap
{
  protected:
    typedef std::map<std::string, EmbedFile> FileMap;
    static FileMap &_themap()
    {
	static FileMap map;
	return map;
    }

  public:
    EmbedMap(const char *filename, const char string[], int len);

    static void all(std::list<EmbedFile> &lst);
    static bool get(const std::string &file, EmbedFile &embed);
};

#endif // __BASE_EMBEDFILE_HH__
