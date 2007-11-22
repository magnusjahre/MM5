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

#ifndef __SYMTAB_HH__
#define __SYMTAB_HH__

#include <map>
#include "targetarch/isa_traits.hh"	// for Addr

class SymbolTable
{
  private:
    typedef std::map<Addr, std::string> ATable;
    typedef std::map<std::string, Addr> STable;

    ATable addrTable;
    STable symbolTable;

  public:
    SymbolTable() {}
    SymbolTable(const std::string &file) { load(file); }
    ~SymbolTable() {}

    bool insert(Addr address, std::string symbol);
    bool load(const std::string &file);

    /// Find the nearest symbol equal to or less than the supplied
    /// address (e.g., the label for the enclosing function).
    /// @param address The address to look up.
    /// @param symbol  Return reference for symbol string.
    /// @param sym_address Return reference for symbol address.
    /// @param next_sym_address Address of following symbol (for
    /// determining valid range of symbol).
    /// @retval True if a symbol was found.
    bool findNearestSymbol(Addr address, std::string &symbol,
			   Addr &sym_address, Addr &next_sym_address) const;

    /// Overload for findNearestSymbol() for callers who don't care
    /// about next_sym_address.
    bool findNearestSymbol(Addr address, std::string &symbol,
			   Addr &sym_address) const
    {
	Addr dummy;
	return findNearestSymbol(address, symbol, sym_address, dummy);
    }


    bool findSymbol(Addr address, std::string &symbol) const;
    bool findAddress(const std::string &symbol, Addr &address) const;
};

/// Global unified debugging symbol table (for target).  Conceptually
/// there should be one of these per System object for full system,
/// and per Process object for non-full-system, but so far one big
/// global one has worked well enough.
extern SymbolTable *debugSymbolTable;

#endif // __SYMTAB_HH__
