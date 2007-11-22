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

#include "base/loader/ecoff_object.hh"

#include "mem/functional/functional.hh"
#include "base/loader/symtab.hh"

#include "base/trace.hh"	// for DPRINTF

#include "base/loader/exec_ecoff.h"
#include "base/loader/coff_sym.h"
#include "base/loader/coff_symconst.h"

using namespace std;

ObjectFile *
EcoffObject::tryFile(const string &fname, int fd, size_t len, uint8_t *data)
{
    if (((ecoff_filehdr *)data)->f_magic == ECOFF_MAGIC_ALPHA) {
	// it's Alpha ECOFF
	return new EcoffObject(fname, fd, len, data,
			       ObjectFile::Alpha, ObjectFile::Tru64);
    }
    else {
	return NULL;
    }
}


EcoffObject::EcoffObject(const string &_filename, int _fd,
			 size_t _len, uint8_t *_data,
			 Arch _arch, OpSys _opSys)
    : ObjectFile(_filename, _fd, _len, _data, _arch, _opSys)
{
    execHdr = (ecoff_exechdr *)fileData;
    fileHdr = &(execHdr->f);
    aoutHdr = &(execHdr->a);

    entry = aoutHdr->entry;

    text.baseAddr = aoutHdr->text_start;
    text.size = aoutHdr->tsize;

    data.baseAddr = aoutHdr->data_start;
    data.size = aoutHdr->dsize;

    bss.baseAddr = aoutHdr->bss_start;
    bss.size = aoutHdr->bsize;

    DPRINTFR(Loader, "text: 0x%x %d\ndata: 0x%x %d\nbss: 0x%x %d\n",
	     text.baseAddr, text.size, data.baseAddr, data.size,
	     bss.baseAddr, bss.size);
}


bool
EcoffObject::loadSections(FunctionalMemory *mem, bool loadPhys)
{
    Addr textAddr = text.baseAddr;
    Addr dataAddr = data.baseAddr;

    if (loadPhys) {
	textAddr &= (ULL(1) << 40) - 1;
	dataAddr &= (ULL(1) << 40) - 1;
    }

    // Since we don't really have an MMU and all memory is
    // zero-filled, there's no need to set up the BSS segment.
    mem->prot_write(textAddr, fileData + ECOFF_TXTOFF(execHdr), text.size);
    mem->prot_write(dataAddr, fileData + ECOFF_DATOFF(execHdr), data.size);

    return true;
}


bool
EcoffObject::loadGlobalSymbols(SymbolTable *symtab)
{
    if (!symtab)
	return false;

    if (fileHdr->f_magic != ECOFF_MAGIC_ALPHA) {
	warn("loadGlobalSymbols: wrong magic on %s\n", filename);
	return false;
    }

    ecoff_symhdr *syms = (ecoff_symhdr *)(fileData + fileHdr->f_symptr);
    if (syms->magic != magicSym2) {
	warn("loadGlobalSymbols: bad symbol header magic on %s\n", filename);
	return false;
    }

    ecoff_extsym *ext_syms = (ecoff_extsym *)(fileData + syms->cbExtOffset);

    char *ext_strings = (char *)(fileData + syms->cbSsExtOffset);
    for (int i = 0; i < syms->iextMax; i++) {
	ecoff_sym *entry = &(ext_syms[i].asym);
	if (entry->iss != -1)
	    symtab->insert(entry->value, ext_strings + entry->iss);
    }

    return true;
}

bool
EcoffObject::loadLocalSymbols(SymbolTable *symtab)
{
    if (!symtab)
	return false;

    if (fileHdr->f_magic != ECOFF_MAGIC_ALPHA) {
	warn("loadGlobalSymbols: wrong magic on %s\n", filename);
	return false;
    }

    ecoff_symhdr *syms = (ecoff_symhdr *)(fileData + fileHdr->f_symptr);
    if (syms->magic != magicSym2) {
	warn("loadGlobalSymbols: bad symbol header magic on %s\n", filename);
	return false;
    }

    ecoff_sym *local_syms = (ecoff_sym *)(fileData + syms->cbSymOffset);
    char *local_strings = (char *)(fileData + syms->cbSsOffset);
    ecoff_fdr *fdesc = (ecoff_fdr *)(fileData + syms->cbFdOffset);

    for (int i = 0; i < syms->ifdMax; i++) {
	ecoff_sym *entry = (ecoff_sym *)(local_syms + fdesc[i].isymBase);
	char *strings = (char *)(local_strings + fdesc[i].issBase);
	for (int j = 0; j < fdesc[i].csym; j++) {
	    if (entry[j].st == stGlobal || entry[j].st == stProc)
		if (entry[j].iss != -1)
		    symtab->insert(entry[j].value, strings + entry[j].iss);
	}
    }

    for (int i = 0; i < syms->isymMax; i++) {
	ecoff_sym *entry = &(local_syms[i]);
	if (entry->st == stProc)
	    symtab->insert(entry->value, local_strings + entry->iss);
    }

    return true;
}
