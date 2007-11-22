#!/usr/bin/env python
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

import getopt, os, os.path, sys
from os.path import join as joinpath, realpath

mypath = sys.path[0]
sys.path.append(joinpath(mypath, '..'))
sys.path.append(joinpath(mypath, '../python'))
sys.path.append(joinpath(mypath, '../util/pbs'))

pathlist = [ '.' ]

m5_build_env = {}

try:
    opts, args = getopt.getopt(sys.argv[1:], '-E:I:')
    for opt,arg in opts:
        if opt == '-E':
            offset = arg.find('=')
            if offset == -1:
                name = arg
                value = 'True'
            else:
                name = arg[:offset]
                value = arg[offset+1:]
            os.environ[name] = value
            m5_build_env[name] = value
        if opt == '-I':
            pathlist.append(arg)
except getopt.GetoptError:
    sys.exit('Improper Usage')

import __main__
__main__.m5_build_env = m5_build_env

from m5 import *

for path in pathlist:
    AddToPath(path)

for arg in args:
    AddToPath(os.path.dirname(arg))
    execfile(arg)

if globals().has_key('root') and isinstance(root, Root):
    instantiate(root)
else:
    print "Instantiation skipped: no root object found."
