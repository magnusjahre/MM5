/*
 * Copyright (c) 2004, 2005
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

#include "base/match.hh"
#include "base/str.hh"

using namespace std;

ObjectMatch::ObjectMatch()
{
}

ObjectMatch::ObjectMatch(const string &expr)
{
    setExpression(expr);
}

void
ObjectMatch::setExpression(const string &expr)
{
    tokens.resize(1);
    tokenize(tokens[0], expr, '.');
}

void
ObjectMatch::setExpression(const vector<string> &expr)
{
    if (expr.empty()) {
	tokens.resize(0);
    } else {
	tokens.resize(expr.size());
	for (int i = 0; i < expr.size(); ++i)
	    tokenize(tokens[i], expr[i], '.');
    }
}

/**
 * @todo this should probably be changed to just use regular
 * expression code
 */
bool
ObjectMatch::domatch(const string &name) const
{
    vector<string> name_tokens;
    tokenize(name_tokens, name, '.');
    int ntsize = name_tokens.size();

    int num_expr = tokens.size();
    for (int i = 0; i < num_expr; ++i) {
	const vector<string> &token = tokens[i];
	int jstop = token.size();

	bool match = true;
	for (int j = 0; j < jstop; ++j) {
	    if (j >= ntsize)
		break;

	    const string &var = token[j];
	    if (var != "*" && var != name_tokens[j]) {
		match = false;
		break;
	    }
	}

	if (match == true)
	    return true;
    }

    return false;
}

