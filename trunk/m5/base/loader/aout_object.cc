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

#include <string>

#include "base/loader/aout_object.hh"

#include "mem/functional/functional.hh"
#include "base/loader/symtab.hh"

#include "base/trace.hh"	// for DPRINTF

#include "base/loader/exec_aout.h"

using namespace std;

ObjectFile *
AoutObject::tryFile(const string &fname, int fd, size_t len, uint8_t *data)
{
    if (!N_BADMAG(*(aout_exechdr *)data)) {
	// right now this is only used for Alpha PAL code
	return new AoutObject(fname, fd, len, data,
			      ObjectFile::Alpha, ObjectFile::UnknownOpSys);
    }
    else {
	return NULL;
    }
}


AoutObject::AoutObject(const string &_filename, int _fd,
		       size_t _len, uint8_t *_data,
		       Arch _arch, OpSys _opSys)
    : ObjectFile(_filename, _fd, _len, _data, _arch, _opSys)
{
    execHdr = (aout_exechdr *)fileData;

    entry = execHdr->entry;

    text.baseAddr = N_TXTADDR(*execHdr);
    text.size = execHdr->tsize;

    data.baseAddr = N_DATADDR(*execHdr);
    data.size = execHdr->dsize;

    bss.baseAddr = N_BSSADDR(*execHdr);
    bss.size = execHdr->bsize;

    DPRINTFR(Loader, "text: 0x%x %d\ndata: 0x%x %d\nbss: 0x%x %d\n",
	     text.baseAddr, text.size, data.baseAddr, data.size,
	     bss.baseAddr, bss.size);
}


bool
AoutObject::loadSections(FunctionalMemory *mem, bool loadPhys)
{
    Addr textAddr = text.baseAddr;
    Addr dataAddr = data.baseAddr;

    if (loadPhys) {
	textAddr &= (ULL(1) << 40) - 1;
	dataAddr &= (ULL(1) << 40) - 1;
    }

    // Since we don't really have an MMU and all memory is
    // zero-filled, there's no need to set up the BSS segment.
    if (text.size != 0)
	mem->prot_write(textAddr, fileData + N_TXTOFF(*execHdr), text.size);
    if (data.size != 0)
	mem->prot_write(dataAddr, fileData + N_DATOFF(*execHdr), data.size);

    return true;
}


bool
AoutObject::loadGlobalSymbols(SymbolTable *symtab)
{
    // a.out symbols not supported yet
    return false;
}

bool
AoutObject::loadLocalSymbols(SymbolTable *symtab)
{
    // a.out symbols not supported yet
    return false;
}
