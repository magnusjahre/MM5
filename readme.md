# Magnus' M5 Simulator

This repository contains the simulator I used throughout my PhD and for a few of my later papers, most notably our [GDP paper at HPCA'18](https://ieeexplore.ieee.org/abstract/document/8327017). The core of the simulator is a branch from M5 but I have substantially modified it over the years. The simulator is no longer actively developed, so it is not a good starting point for new research projects.

## Setup Tips

The only missing dependency for a recent default ubutnu seems to be libz. Install with:
```
sudo apt-get install zlib1g-dev
```

If you want to try to run MM5, it is useful to know that it expects a few environment variables. The below code assumes that you got hold of CAL's pre-compiled benchmarks and cloned both MM5 and MM5-tools into your home directory. If so, adding the below code to your .bashrc file will save you some confusing debugging.

```
export PYTHONPATH=$PYTHONPATH:$HOME/MM5-tools
export BMROOT=$HOME/benchmarks
export SIMROOT=$HOME/MM5
```
