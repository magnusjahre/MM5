from m5 import *
from FullCPU import FullCPU
from BaseCache import BaseCache

class SharedStallHeuristic(Enum): vals = ['shared-exists', 'rob', 'rob-write']

class MemoryOverlapEstimator(SimObject):
    type = 'MemoryOverlapEstimator'
    id = Param.Int("ID")
    interference_manager = Param.InterferenceManager("Interference Manager Object")
    cpu_count = Param.Int("Number of CPUs")
    shared_stall_heuristic = Param.SharedStallHeuristic("The heuristic that decides if a processor stall is due to a shared event")
    shared_req_trace_enabled = Param.Bool("Trace all requests (warning: will create large files)")
    graph_analysis_enabled = Param.Bool("Analyze miss graph to determine data (warning: performance overhead)")
    overlapTable = Param.MemoryOverlapTable("Overlap table")
    trace_sample_id = Param.Int("The id of the sample to trace, traces all if -1 (default)")
    cpl_table_size = Param.Int("The size of the CPL table")
    itca = Param.ITCA("A pointer to the class implementing the ITCA accounting scheme")
    bois_stall_trace_enabled = Param.Bool("Trace all Du Bois stall data (warning: will create large files)")
    graph_cpl_enabled = Param.Bool("Analyse the complete dependency graph (warning: significant memory and performance overhead)")
    