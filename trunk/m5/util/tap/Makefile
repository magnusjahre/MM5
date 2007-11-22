CC= gcc
CXX= g++

CURDIR?= $(shell /bin/pwd)
SRCDIR?= .
M5_SRCDIR?= $(SRCDIR)/../..

vpath % $(M5_SRCDIR)/base
vpath % $(M5_SRCDIR)/sim

INCLDIRS= -I. -I$(M5_SRCDIR) -I- -I/usr/local/include
CCFLAGS= -g -O0 -MMD $(INCLDIRS) 

default: m5tap

m5tap: tap.o cprintf.o
	$(CXX) $(LFLAGS) -o $@ $^ -lpcap -L/usr/local/lib -ldnet 

install: m5tap
	$(SUDO) install -o root -m 555 m5tap /usr/local/bin

clean:
	@rm -f m5tap *.o *.d *~ .#*

.PHONY: clean

# C++ Compilation
%.o: %.cc
	@echo '$(CXX) $(CCFLAGS) -c $(notdir $<) -o $@'
	@$(CXX) $(CCFLAGS) -c $< -o $@

# C Compilation
%.o: %.c
	@echo '$(CC) $(CCFLAGS) -c $(notdir $<) -o $@'
	@$(CC) $(CCFLAGS) -c $< -o $@

-include *.d
