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

#include <iostream>
#include <fstream>
#include <string>
#include <vector>

#include "sim/host.hh"
#include "base/misc.hh"
#include "base/str.hh"
#include "base/loader/symtab.hh"

using namespace std;

SymbolTable *debugSymbolTable = NULL;

bool
SymbolTable::insert(Addr address, string symbol)
{
    if (!addrTable.insert(make_pair(address, symbol)).second)
	return false;

    if (!symbolTable.insert(make_pair(symbol, address)).second)
	return false;

    return true;
}


bool
SymbolTable::load(const string &filename)
{
    string buffer;
    ifstream file(filename.c_str());

    if (!file) {
	cerr << "Can't open symbol table file " << filename << endl;
	fatal("file error");
    }

    while (!file.eof()) {
	getline(file, buffer);
	if (buffer.empty())
	    continue;

	int idx = buffer.find(',');
	if (idx == string::npos)
	    return false;

	string address = buffer.substr(0, idx);
	eat_white(address);
	if (address.empty())
	    return false;

	string symbol = buffer.substr(idx + 1);
	eat_white(symbol);
	if (symbol.empty())
	    return false;

	Addr addr;
	if (!to_number(address, addr))
	    return false;

	if (!insert(addr, symbol))
	    return false;
    }

    file.close();

    return true;
}

bool
SymbolTable::findNearestSymbol(Addr address, string &symbol,
			       Addr &sym_address, Addr &next_sym_address) const
{
    // find first key *larger* than desired address
    ATable::const_iterator i = addrTable.upper_bound(address);

    // if very first key is larger, we're out of luck
    if (i == addrTable.begin())
	return false;

    next_sym_address = i->first;
    --i;
    sym_address = i->first;
    symbol = i->second;

    return true;
}

bool
SymbolTable::findSymbol(Addr address, string &symbol) const
{
    ATable::const_iterator i = addrTable.find(address);
    if (i == addrTable.end())
	return false;

    symbol = (*i).second;
    return true;
}

bool
SymbolTable::findAddress(const string &symbol, Addr &address) const
{
    STable::const_iterator i = symbolTable.find(symbol);
    if (i == symbolTable.end())
	return false;

    address = (*i).second;
    return true;
}
