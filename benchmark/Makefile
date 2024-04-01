CC = gcc
CFLAGS = -g -w -m32
LDFLAGS = -L.. -l:my_vm.o -lm

all: clean test

test: test.o
	$(CC) $(CFLAGS) -o test test.o $(LDFLAGS)

test.o: test.c
	$(CC) $(CFLAGS) -c test.c

clean:
	rm -rf test *.o *.dSYM