from m5 import *

class AdaptiveMHA(SimObject):
    type = 'AdaptiveMHA'
    lowThreshold = Param.Float("Threshold for increasing the number of MSHRs")
    highThreshold = Param.Float("Threshold for reducing the number of MSHRs")
    cpuCount = Param.Int("Number of cores in the CMP")
    sampleFrequency = Param.Tick("The number of clock cycles between each sample")
    startTick = Param.Tick("The tick where the scheme is started")
    onlyTraceBus = Param.Bool("Only create the bus trace, adaptiveMHA is turned off")
    neededRepeats = Param.Int("Number of repeated desicions to change config")
    staticAsymmetricMHA = VectorParam.Int("The number of times each caches mshrcount should be reduced")