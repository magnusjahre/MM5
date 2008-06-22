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
    useFairMHA = Param.Bool("True if the fair AMHA implementation should be used")
    resetCounter = Param.Int("The number of events that should be processed before F-AMHA is reset")
    reductionThreshold = Param.Float("The percentage reduction in interference points needed to accept a reduction")
    minInterferencePointAllowed = Param.Float("Lowest relative interference point that will count as interference")