#!/usr/bin/env python

# Copyright (c) 2004, 2005
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
# so long as the name of The University of Michigan is not used in
# any advertising or publicity pertaining to the use or distribution
# of this software without specific, written prior authorization.
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


#
# This file generates the header and source files for the flags
# that control the tracing facility.
#

import sys

if len(sys.argv) != 2:
    print "%s: Need argument (basename of cc/hh files)" % sys.argv[0]
    sys.exit(1)

hhfilename = sys.argv[1] + '.hh'
ccfilename = sys.argv[1] + '.cc'

#
# The list of trace flags that can be used to condition DPRINTFs etc.
# To define a new flag, simply add it to this list.
#
baseFlags = [
    'TCPIP',
    'Bus',
    'ScsiDisk',
    'ScsiCtrl',
    'ScsiNone',
    'DMA',
    'DMAReadVerbose',
    'DMAWriteVerbose',
    'TLB',
    'SimpleDisk',
    'SimpleDiskData',
    'Clock',
    'Regs',
    'MC146818',
    'IPI',
    'Timer',
    'Mbox',
    'PCIA',
    'PCIDEV',
    'PciConfigAll',
    'ISP',
    'BADADDR',
    'Console',
    'ConsolePoll',
    'ConsoleVerbose',
    'AlphaConsole',
    'Flow',
    'Interrupt',
    'Fault',
    'Cycle',
    'Loader',
    'MMU',
    'Ethernet',
    'EthernetPIO',
    'EthernetDMA',
    'EthernetData',
    'EthernetDesc',
    'EthernetIntr',
    'EthernetSM',
    'EthernetCksum',
    'GDBMisc',
    'GDBAcc',
    'GDBRead',
    'GDBWrite',
    'GDBSend',
    'GDBRecv',
    'GDBExtra',
    'VtoPhys',
    'Printf',
    'DebugPrintf',
    'Serialize',
    'Event',
    'PCEvent',
    'SyscallWarnings',
    'SyscallVerbose',
    'DiskImage',
    'DiskImageRead',
    'DiskImageWrite',
    'InstExec',
    'BPredRAS',
    'Cache',
    'IIC',
    'IICMore',
    'MSHR',
    'Chains',
    'Pipeline',
    'Stats',
    'StatEvents',
    'Context',
    'Config',
    'Sampler',
    'WriteBarrier',
    'IdeCtrl',
    'IdeDisk',
    'Tsunami',
    'Uart',
    'Split',
    'SQL',
    'Thread',
    'Fetch',
    'Decode',
    'Rename',
    'IEW',
    'Commit',
    'IQ',
    'ROB',
    'FreeList',
    'RenameMap',
    'LDSTQ',
    'StoreSet',
    'MemDepUnit',
    'DynInst',
    'FullCPU',
    'CommitRate',
    'OoOCPU',
    'HWPrefetch',
    'UniformPartitioning',
    'AddrBusVerify',
    'DataBusVerify',
    'MemoryController',
    'DRAM',
    'Blocking',
    'CachePartitioning',
    'Crossbar',
    'MemoryControllerInterference',
    'MissBWPolicy',
    'MissBWPolicyExtra',
    'FuncMem',
    'OverlapEstimator'
    ]

#
# "Compound" flags correspond to a set of base flags.  These exist
# solely for convenience in setting them via the command line: if a
# compound flag is specified, all of the corresponding base flags are
# set.  Compound flags cannot be used directly in DPRINTFs etc.
# To define a new compound flag, add a new entry to this hash
# following the existing examples.
#
compoundFlagMap = {
    'GDBAll' : [ 'GDBMisc', 'GDBAcc', 'GDBRead', 'GDBWrite', 'GDBSend', 'GDBRecv', 'GDBExtra' ],
    'ScsiAll' : [ 'ScsiDisk', 'ScsiCtrl', 'ScsiNone' ],
    'DiskImageAll' : [ 'DiskImage', 'DiskImageRead', 'DiskImageWrite' ],
    'EthernetAll' : [ 'Ethernet', 'EthernetPIO', 'EthernetDMA', 'EthernetData' , 'EthernetDesc', 'EthernetIntr', 'EthernetSM', 'EthernetCksum' ],
    'EthernetNoData' : [ 'Ethernet', 'EthernetPIO', 'EthernetDesc', 'EthernetIntr', 'EthernetSM', 'EthernetCksum' ],	
    'IdeAll' : [ 'IdeCtrl', 'IdeDisk' ],
    'FullCPUAll' : [ 'Fetch', 'Decode', 'Rename', 'IEW', 'Commit', 'IQ', 'ROB', 'FreeList', 'RenameMap', 'LDSTQ', 'StoreSet', 'MemDepUnit', 'DynInst', 'FullCPU'],
    'MissBWPolicyVerbose': ['MissBWPolicy', 'MissBWPolicyExtra']    
}

#############################################################
#
# Everything below this point generates the appropriate C++
# declarations and definitions for the trace flags.  If you are simply
# adding or modifying flag definitions, you should not have to change
# anything below.
#

import sys

# extract just the compound flag names into a list
compoundFlags = []
compoundFlags.extend(compoundFlagMap.keys())
compoundFlags.sort()

#
# First generate the header file.  This defines the Flag enum
# and some extern declarations for the .cc file.
#
try:
    hhfile = file(hhfilename, 'w')
except IOError, e:
    sys.exit("can't open %s: %s" % (hhfilename, e))

# file header boilerplate
print >>hhfile, '''
/*
 * DO NOT EDIT THIS FILE!
 *
 * Automatically generated from traceflags.py
 */

#ifndef __BASE_TRACE_FLAGS_HH__
#define __BASE_TRACE_FLAGS_HH__

namespace Trace {

enum Flags {
''',

# Generate the enum.  Base flags come first, then compound flags.
idx = 0
for flag in baseFlags:
    print >>hhfile, '    %s = %d,' % (flag, idx)
    idx += 1

numBaseFlags = idx
print >>hhfile, '    NumFlags = %d,' % idx

# put a comment in here to separate base from compound flags
print >>hhfile, '''
    // The remaining enum values are *not* valid indices for Trace::flags.
    // They are "compound" flags, which correspond to sets of base
    // flags, and are used only by TraceParamContext::setFlags().
''',

for flag in compoundFlags:
    print >>hhfile, '    %s = %d,' % (flag, idx)
    idx += 1

numCompoundFlags = idx - numBaseFlags
print >>hhfile, '    NumCompoundFlags = %d' % numCompoundFlags

# trailer boilerplate
print >>hhfile, '''\
}; // enum Flags

// Array of strings for SimpleEnumParam
extern const char *flagStrings[];
extern const int numFlagStrings;

// Array of arraay pointers: for each compound flag, gives the list of
// base flags to set.  Inidividual flag arrays are terminated by -1.
extern const Flags *compoundFlags[];

/* namespace Trace */ }

#endif // __BASE_TRACE_FLAGS_HH__
''',

hhfile.close()

#
#
# Print out .cc file with array definitions.
#
#
try:
    ccfile = file(ccfilename, 'w')
except OSError, e:
    sys.exit("can't open %s: %s" % (ccfilename, e))

# file header
print >>ccfile, '''
/*
 * DO NOT EDIT THIS FILE!
 *
 * Automatically generated from traceflags.pl.
 */

#include "base/traceflags.hh"

using namespace Trace;

const char *Trace::flagStrings[] =
{
''',

# The string array is used by SimpleEnumParam to map the strings
# provided by the user to enum values.
for flag in baseFlags:
    print >>ccfile, '    "%s",' % flag

for flag in compoundFlags:
    print >>ccfile, '    "%s",' % flag

print >>ccfile, '};\n'

numFlagStrings = len(baseFlags) + len(compoundFlags);

print >>ccfile, 'const int Trace::numFlagStrings = %d;' % numFlagStrings
print >>ccfile

#
# Now define the individual compound flag arrays.  There is an array
# for each compound flag listing the component base flags.
#

for flag in compoundFlags:
    flags = compoundFlagMap[flag]
    flags.append('(Flags)-1')
    print >>ccfile, 'static const Flags %sMap[] =' % flag
    print >>ccfile, '{ %s };' % (', '.join(flags))
    print >>ccfile

#
# Finally the compoundFlags[] array maps the compound flags
# to their individual arrays/
#
print >>ccfile, 'const Flags *Trace::compoundFlags[] ='
print >>ccfile, '{'

for flag in compoundFlags:
    print >>ccfile, '    %sMap,' % flag

# file trailer
print >>ccfile, '};'

ccfile.close()

