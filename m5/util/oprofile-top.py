#! /usr/bin/env python

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
# redistribute this software and such derivative works for any purpose,
# so long as the copyright notice above, this grant of permission, and
# the disclaimer below appear in all copies made; and so long as the
# name of The University of Michigan is not used in any advertising or
# publicity pertaining to the use or distribution of this software
# without specific, written prior authorization.
#
# THIS SOFTWARE IS PROVIDED AS IS, WITHOUT REPRESENTATION FROM THE
# UNIVERSITY OF MICHIGAN AS TO ITS FITNESS FOR ANY PURPOSE, AND WITHOUT
# WARRANTY BY THE UNIVERSITY OF MICHIGAN OF ANY KIND, EITHER EXPRESS OR
# IMPLIED, INCLUDING WITHOUT LIMITATION THE IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. THE REGENTS OF
# THE UNIVERSITY OF MICHIGAN SHALL NOT BE LIABLE FOR ANY DAMAGES,
# INCLUDING DIRECT, SPECIAL, INDIRECT, INCIDENTAL, OR CONSEQUENTIAL
# DAMAGES, WITH RESPECT TO ANY CLAIM ARISING OUT OF OR IN CONNECTION
# WITH THE USE OF THE SOFTWARE, EVEN IF IT HAS BEEN OR IS HEREAFTER
# ADVISED OF THE POSSIBILITY OF SUCH DAMAGES.

# Parse sampled function profile output (quick hack).

import sys
import re
import getopt
from categories import *

def category(app,sym):
    if re.search("vmlinux-2.6", app):
        name = sym
    else:
        name = app

    if categories.has_key(name):
        return categories[name]
    for regexp, cat in categories_re:
        if regexp.match(name):
            return cat
    print "no match for symbol %s" % name
    return 'other'

try:
   (opts, files) = getopt.getopt(sys.argv[1:], 'i')
except getopt.GetoptError:
        print "usage", sys.argv[0], "[-i] <files>"
        sys.exit(2)

showidle = True

for o,v in opts:
    if o == "-i":
        showidle = False       
print files    
f = open(files.pop())
total = 0
prof = {}
linenum  = 0
for line in f.readlines():
    line = re.sub("\(no symbols\)", "nosym", line)
    line = re.sub("anonymous.*", "nosym", line)
    linenum += 1
    if linenum < 4:
        continue    
    (count, percent, app, sym) = line.split()
    #total += int(count)
    cat = category(app,sym)
    if cat != 'idle' or showidle:
      total += int(count)
      prof[cat] = prof.get(cat,0) + int(count)

cats = ['other', 'user', 'copy', 'bufmgt', 'stack', 'driver', 'interrupt', 'alignment' ] 
    
if showidle:
   cats.insert(0,'idle') 

#syms = [(i[1], i[0]) for i in prof.items()]
#syms.sort()
#for i in range(len(syms)):
#    print "%s -- %5.1f%% " % (prof[i][1], 100 * float(prof[i][0])/float(total))

for d in cats:
    if prof.has_key(d):
        print "%s -- %5.1f%% " % (d, 100 * float(prof[d])/float(total))
        
