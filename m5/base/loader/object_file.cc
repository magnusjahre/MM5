/*
 * Copyright (c) 2002, 2003, 2004
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

#include <list>
#include <string>

#include <sys/types.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

#include "base/cprintf.hh"
#include "base/loader/object_file.hh"
#include "base/loader/symtab.hh"

#include "base/loader/ecoff_object.hh"
#include "base/loader/aout_object.hh"
#include "base/loader/elf_object.hh"

using namespace std;

ObjectFile::ObjectFile(const string &_filename, int _fd,
		       size_t _len, uint8_t *_data,
		       Arch _arch, OpSys _opSys)
    : filename(_filename), descriptor(_fd), fileData(_data), len(_len),
      arch(_arch), opSys(_opSys)
{
}


ObjectFile::~ObjectFile()
{
    close();
}


void
ObjectFile::close()
{
    if (descriptor >= 0) {
	::close(descriptor);
	descriptor = -1;
    }

    if (fileData) {
	::munmap(fileData, len);
	fileData = NULL;
    }
}


ObjectFile *
createObjectFile(const string &fname)
{
    // open the file
    int fd = open(fname.c_str(), O_RDONLY);
    if (fd < 0) {
	return NULL;
    }

    // find the length of the file by seeking to the end
    size_t len = (size_t)lseek(fd, 0, SEEK_END);

    // mmap the whole shebang
    uint8_t *fileData =
	(uint8_t *)mmap(NULL, len, PROT_READ, MAP_SHARED, fd, 0);
    if (fileData == MAP_FAILED) {
	close(fd);
	return NULL;
    }

    ObjectFile *fileObj = NULL;

    // figure out what we have here
    if ((fileObj = EcoffObject::tryFile(fname, fd, len, fileData)) != NULL) {
	return fileObj;
    }

    if ((fileObj = AoutObject::tryFile(fname, fd, len, fileData)) != NULL) {
	return fileObj;
    }

    if ((fileObj = ElfObject::tryFile(fname, fd, len, fileData)) != NULL) {
	return fileObj;
    }

    // don't know what it is
    close(fd);
    munmap(fileData, len);
    return NULL;
}
