GCC=cc
CFLAGS+=-Wall -g
CFLAGS+=-DUSE_SLEEP
SRC=$(wildcard *.c)
PROGS=$(patsubst %.c,%,$(SRC))

%.o: %.c
	$(GCC) $(CFLAGS+) $< -o $@

all: $(PROGS)

clean:	
	rm -f *.core $(PROGS) *~
