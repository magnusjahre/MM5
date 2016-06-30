
from m5 import *
from BaseCPU import BaseCPU

class OpType(Enum):
    vals = ['(null)', 'IntAlu', 'IntMult', 'IntDiv', 'FloatAdd',
            'FloatCmp', 'FloatCvt', 'FloatMult', 'FloatDiv', 'FloatSqrt',
            'MemRead', 'MemWrite', 'IprAccess', 'InstPrefetch']

class OpDesc(SimObject):
    type = 'OpDesc'
    issueLat = Param.Int(1, "cycles until another can be issued")
    opClass = Param.OpType("type of operation")
    opLat = Param.Int(1, "cycles until result is available")

class FUDesc(SimObject):
    type = 'FUDesc'
    count = Param.Int("number of these FU's available")
    opList = VectorParam.OpDesc("operation classes for this FU type")

class FuncUnitPool(SimObject):
    type = 'FuncUnitPool'
    FUList = VectorParam.FUDesc("list of FU's for this pool")

class BaseIQ(SimObject):
    type = 'BaseIQ'
    abstract = True
    caps = VectorParam.Int("IQ caps")

class StandardIQ(BaseIQ):
    type = 'StandardIQ'
    prioritized_issue = Param.Bool(False, "thread priorities in issue")
    size = Param.Int("number of entries")

class SegmentedIQ(BaseIQ):
    type = 'SegmentedIQ'
    en_thread_priority = Param.Bool("enable thread priority")
    max_chain_depth = Param.Int("max chain depth")
    num_segments = Param.Int("number of IQ segments")
    segment_size = Param.Int("segment size")
    segment_thresh = Param.Int("segment delta threshold")
    use_bypassing = Param.Bool(True, "enable bypass at dispatch")
    use_pipelined_prom = Param.Bool(True, "enable pipelined chain wires")
    use_pushdown = Param.Bool(True, "enable instruction pushdown")

class SeznecIQ(BaseIQ):
    type = 'SeznecIQ'
    issue_buf_size = Param.Int("number of issue buffer entries")
    line_size = Param.Int("number of insts per prescheduling line")
    num_lines = Param.Int("number of prescheduling lines")
    use_hm_predictor = Param.Bool("use hit/miss predictor")

class FullCPU(BaseCPU):
    type = 'FullCPU'
    branch_pred = Param.BranchPred("branch predictor object")
    class ChainPolicy(Enum):
        vals = ['OneToOne', 'Static', 'StaticStall', 'Dynamic']
    chain_wire_policy = Param.ChainPolicy('OneToOne',
                                          "chain-wire assignment policy")
    class CommitModel(Enum): vals = ['smt', 'perthread', 'sscalar', 'rr']
    commit_model = Param.CommitModel('smt',"commit model")
    commit_width = Param.Int(Self.width,
			     "instruction commit BW (insts/cycle)")
    decode_to_dispatch = Param.Int("decode to dispatch latency (cycles)")
    decode_width = Param.Int(Self.width,
			     "instruction decode BW (insts/cycle)")
    class MemDisambig(Enum): vals = ['conservative', 'normal', 'oracle']
    disambig_mode = Param.MemDisambig('normal',
        "memory address disambiguation mode")
    class DispatchPolicy(Enum): vals = ['mod_n', 'perqueue', 'dependence']
    dispatch_policy = Param.DispatchPolicy('mod_n',
        "method for selecting destination IQ")
    dispatch_to_issue = Param.Int(1,
        "minimum dispatch to issue latency (cycles)")
    fault_handler_delay = Param.Int(5,
	"latency from commit of faulting inst to fetch of handler")
    fetch_branches = Param.Int("stop fetching after 'n'-th branch")
    class FetchPolicy(Enum):
        vals = ['RR', 'IC', 'ICRC', 'Ideal', 'Conf', 'Redundant',
                'RedIC', 'RedSlack', 'Rand']
    fetch_policy = Param.FetchPolicy('IC', "SMT fetch policy")
    fetch_pri_enable = Param.Bool(False, "use thread priorities in fetch")
    fetch_width = Param.Int(Self.width, "instruction fetch BW (insts/cycle)")
    fupools = VectorParam.FuncUnitPool("list of FU pools")
    icount_bias = VectorParam.Int([], "per-thread static icount bias")
    ifq_size = Param.Int("instruction fetch queue size (in insts)")
    inorder_issue = Param.Bool(False, "issue instruction inorder")
    iq = VectorParam.BaseIQ("instruction queue object")
    iq_comm_latency = Param.Int(1,
        "writeback communication latency (cycles) for multiple IQ's")
    issue_bandwidth = VectorParam.Int([], "maximum per-thread issue rate")
    issue_width = Param.Int(Self.width, "instruction issue B/W (insts/cycle)")
    lines_to_fetch = Param.Int(999, "instruction fetch BW (lines/cycle)")
    loose_mod_n_policy = Param.Bool(True,
        "loosen the Mod-N dispatch policy")
    lsq_size = Param.Int("load/store queue size")
    max_chains = Param.Int(64, "maximum number of dependence chains")
    max_wires = Param.Int(64, "maximum number of dependence chain wires")
    mispred_recover = Param.Int("branch misprediction recovery latency")
    mt_frontend = Param.Bool(True, "use the multi-threaded IFQ and FTDQ")
    num_icache_ports = Param.Int("number of icache ports")
    num_threads = Param.Int(0,
        "number of HW thread contexts (0 = # of processes)")
    pc_sample_interval = Param.Latency('0ns', "PC sample interval")
    prioritized_commit = Param.Bool(False,
        "use thread priorities in commit")
    prioritized_issue = Param.Bool(False, "issue HP thread first")
    ptrace = Param.PipeTrace(NULL, "pipeline tracing object")
    rob_caps = VectorParam.Int([], "maximum per-thread ROB occupancy")
    rob_size = Param.Int("reorder buffer size")
    storebuffer_size = Param.Int("store buffer size")
    class PrefetchPolicy(Enum):
        vals = ['enable', 'disable', 'squash']
    sw_prefetch_policy = Param.PrefetchPolicy('enable',
        "software prefetch policy")
    thread_weights = VectorParam.Int([], "issue priority weights")
    use_hm_predictor = Param.Bool(False, "enable hit/miss predictor")
    use_lat_predictor = Param.Bool(False, "enable latency predictor")
    use_lr_predictor = Param.Bool(True, "enable left/right predictor")
    width = Param.Int(4, "default machine width")
    adaptiveMHA = Param.AdaptiveMHA("Adaptive MHA object")
    interferenceManager = Param.InterferenceManager("The interference manager")
    basePolicy = Param.BasePolicy("base policy pointer")
    quit_on_cpu_id = Param.Int("Quit when this CPU reaches a certain number of instructions")
    overlapEstimator = Param.MemoryOverlapEstimator("The overlap estimator")
    commit_trace_frequency = Param.Int("commit trace frequency in committed instructions")
    commit_trace_instructions = VectorParam.Int("Array of instruction counts to sample at")
