# Copyright (c) 2005
# The Regents of The University of Michigan
# All Rights Reserved
#
# This code is part of the M5 simulator, developed by Nathan Binkert,
# Erik Hallnor, Steve Raasch, and Steve Reinhardt, with contributions
# from Ron Dreslinski, Dave Greene, Lisa Hsu, Kevin Lim, Ali Saidi,
# and Andrew Schultz.
#
# Permission is granted to use, copy, create derivative works and
# redistribute this software and such derivative works for any
# purpose, so long as the copyright notice above, this grant of
# permission, and the disclaimer below appear in all copies made; and
# so long as the name of The University of Michigan is not used in any
# advertising or publicity pertaining to the use or distribution of
# this software without specific, written prior authorization.
#
# THIS SOFTWARE IS PROVIDED AS IS, WITHOUT REPRESENTATION FROM THE
# UNIVERSITY OF MICHIGAN AS TO ITS FITNESS FOR ANY PURPOSE, AND
# WITHOUT WARRANTY BY THE UNIVERSITY OF MICHIGAN OF ANY KIND, EITHER
# EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE. THE REGENTS OF THE UNIVERSITY OF MICHIGAN SHALL NOT BE
# LIABLE FOR ANY DAMAGES, INCLUDING DIRECT, SPECIAL, INDIRECT,
# INCIDENTAL, OR CONSEQUENTIAL DAMAGES, WITH RESPECT TO ANY CLAIM
# ARISING OUT OF OR IN CONNECTION WITH THE USE OF THE SOFTWARE, EVEN
# IF IT HAS BEEN OR IS HEREAFTER ADVISED OF THE POSSIBILITY OF SUCH
# DAMAGES.

# SCons helper functions

# Take a mish-mash of lists and dicts (as found in env['CPPDEFINES'])
# and generate a flat dict of keyword->value mappings.  Values are
# coerced to strings.  Keywords that are defined but without a value
# are given the value 'True'.
def flatten_defines(defines):
    mydict = {}
    if isinstance(defines, list):
        for var in defines:
            if isinstance(var, tuple):
                mydict[var[0]] = str(var[1])
            else:
                mydict[var] = 'True'

    elif isinstance(defines, dict):
        for key,val in defines.iteritems():
            mydict[key] = str(val)

    else:
        panic("invalid type in flatten_defines: %s" % type(defines))

    return mydict
