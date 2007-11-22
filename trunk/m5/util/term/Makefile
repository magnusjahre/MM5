CC= gcc
CCFLAGS= -g -O0

default: m5term

m5term: term.c
	$(CC) $(LFLAGS) -o $@ $^

install: m5term
	$(SUDO) install -o root -m 555 m5term /usr/local/bin

clean:
	@rm -f m5term *~ .#*

.PHONY: clean
