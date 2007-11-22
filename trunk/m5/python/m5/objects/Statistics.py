from m5 import *
class Statistics(ParamContext):
    type = 'Statistics'
    descriptions = Param.Bool(True, "display statistics descriptions")
    project_name = Param.String('test',
        "project name for statistics comparison")
    simulation_name = Param.String('test',
        "simulation name for statistics comparison")
    simulation_sample = Param.String('0', "sample for stats aggregation")
    text_file = Param.String('m5stats.txt', "file to dump stats to")
    text_compat = Param.Bool(True, "simplescalar stats compatibility")
    mysql_db = Param.String('', "mysql database to put data into")
    mysql_user = Param.String('', "username for mysql")
    mysql_password = Param.String('', "password for mysql user")
    mysql_host = Param.String('', "host for mysql")
    #events_start = Param.Tick(MaxTick, "cycle to start tracking events")
    dump_reset = Param.Bool(False, "when dumping stats, reset afterwards")
    dump_cycle = Param.Tick(0, "cycle on which to dump stats")
    dump_period = Param.Tick(0, "period with which to dump stats")
    ignore_events = VectorParam.String([], "name strings to ignore")

