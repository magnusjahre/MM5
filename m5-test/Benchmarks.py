from m5 import *
import os

# Base for tests is directory containing this file.
test_base = os.path.dirname(__file__)

anagram_bin = os.path.join(test_base, 'test-progs/anagram/bin')
gcc_bin = os.path.join(test_base, 'test-progs/gcc/bin')

# Live Processes
class Anagram(LiveProcess):
    executable = os.path.join(anagram_bin, 'anagram')
    cmd = 'anagram'

class GCC(LiveProcess):
    executable = os.path.join(gcc_bin, 'cc1_peak.ev6')
    cmd = 'cc1_peak.ev6'

class Radix(LiveProcess):
    executable = os.path.join(test_base, 'test-progs/radix/bin/radix')
    cmd = 'radix -n262144 -p4'

# Checkpointed EIO Processes
class AnagramShort(EioProcess):
    file = os.path.join(anagram_bin, 'anagram-vshort.eio.gz')

class AnagramLong(EioProcess):
    file = os.path.join(anagram_bin, 'anagram-long.eio.gz')

class AnagramLongCP(AnagramLong):
    chkpt = os.path.join(anagram_bin, 'anagram-long.cp1.gz')

class AnagramFull(EioProcess):
    file = os.path.join(anagram_bin, 'anagram-full.eio.gz')

class GCCShort(EioProcess):
    file = os.path.join(gcc_bin, 'cc1-short.eio.gz')

class GCCLong(EioProcess):
    file = os.path.join(gcc_bin, 'cc1-cccp.eio.gz')

class GCCLongCP(GCCLong):
    chkpt = os.path.join(gcc_bin, 'cc1-cccp.cp1.gz')
