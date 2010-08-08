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

#ifndef __OBJECT_FILE_HH__
#define __OBJECT_FILE_HH__

#include "targetarch/isa_traits.hh"	// for Addr

class FunctionalMemory;
class SymbolTable;

class ObjectFile
{
  public:

    enum Arch {
	UnknownArch,
	Alpha
    };

    enum OpSys {
	UnknownOpSys,
	Tru64,
	Linux
    };

  protected:
    const std::string filename;
    int descriptor;
    uint8_t *fileData;
    size_t len;

    Arch  arch;
    OpSys opSys;

    ObjectFile(const std::string &_filename, int _fd,
	       size_t _len, uint8_t *_data,
	       Arch _arch, OpSys _opSys);

  public:
    virtual ~ObjectFile();

    void close();

    virtual bool loadSections(FunctionalMemory *mem,
			      bool loadPhys = false) = 0;
    virtual bool loadGlobalSymbols(SymbolTable *symtab) = 0;
    virtual bool loadLocalSymbols(SymbolTable *symtab) = 0;

    Arch  getArch()  const { return arch; }
    OpSys getOpSys() const { return opSys; }

  protected:

    struct Section {
	Addr baseAddr;
	size_t size;
    };

    Addr entry;
    Addr globalPtr;

    Section text;
    Section data;
    Section bss;

    Addr _programHeaderTable;
    uint16_t _programHeaderSize;
    uint16_t _programHeaderCount;

  public:
    Addr entryPoint() const { return entry; }
    Addr globalPointer() const { return globalPtr; }

    Addr textBase() const { return text.baseAddr; }
    Addr dataBase() const { return data.baseAddr; }
    Addr bssBase() const { return bss.baseAddr; }

    size_t textSize() const { return text.size; }
    size_t dataSize() const { return data.size; }
    size_t bssSize() const { return bss.size; }

    Addr getHeaderTable(){
    	return _programHeaderTable;
    }

    uint16_t getHeaderCount(){
    	return _programHeaderCount;
    }
};

ObjectFile *createObjectFile(const std::string &fname);


#endif // __OBJECT_FILE_HH__
