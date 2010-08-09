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

#ifndef __ELF_OBJECT_HH__
#define __ELF_OBJECT_HH__

#include "base/loader/object_file.hh"
#include <set>
#include <vector>

class ElfObject : public ObjectFile
{
  protected:

    uint8_t *fileTextBits; //!< Pointer to file's text segment image
    uint8_t *fileDataBits; //!< Pointer to file's data segment image
    std::set<std::string> sectionNames;

    /// Helper functions for loadGlobalSymbols() and loadLocalSymbols().
    bool loadSomeSymbols(SymbolTable *symtab, int binding);

    ElfObject(const std::string &_filename, int _fd,
	      size_t _len, uint8_t *_data,
	      Arch _arch, OpSys _opSys);

    void getSections();
    bool sectionExists(std::string sec);

  public:
    virtual ~ElfObject() {}

    virtual bool loadSections(FunctionalMemory *mem,
			      bool loadPhys = false);
    virtual bool loadGlobalSymbols(SymbolTable *symtab);
    virtual bool loadLocalSymbols(SymbolTable *symtab);

    static ObjectFile *tryFile(const std::string &fname, int fd,
			       size_t len, uint8_t *data);

    virtual bool isDynamic() { return sectionExists(".interp"); }
};

#endif // __ELF_OBJECT_HH__
