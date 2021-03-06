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

#ifndef __ECOFF_OBJECT_HH__
#define __ECOFF_OBJECT_HH__

#include "base/loader/object_file.hh"

// forward decls: avoid including exec_ecoff.h here
struct ecoff_exechdr;
struct ecoff_filehdr;
struct ecoff_aouthdr;

class EcoffObject : public ObjectFile
{
  protected:
    ecoff_exechdr *execHdr;
    ecoff_filehdr *fileHdr;
    ecoff_aouthdr *aoutHdr;

    EcoffObject(const std::string &_filename, int _fd,
		size_t _len, uint8_t *_data,
		Arch _arch, OpSys _opSys);

  public:
    virtual ~EcoffObject() {}

    virtual bool loadSections(FunctionalMemory *mem,
			      bool loadPhys = false);
    virtual bool loadGlobalSymbols(SymbolTable *symtab);
    virtual bool loadLocalSymbols(SymbolTable *symtab);

    static ObjectFile *tryFile(const std::string &fname, int fd,
			       size_t len, uint8_t *data);
};

#endif // __ECOFF_OBJECT_HH__
