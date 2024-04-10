CC = gcc
CFLAGS = -g -w -m32 -fsanitize=address,undefined
LDFLAGS = -L.. -l:my_vm.o -lm

all: clean test testString

test: test.o
	$(CC) $(CFLAGS) -o test test.o $(LDFLAGS)

test.o: test.c
	$(CC) $(CFLAGS) -c test.c

testString: testString.o
	$(CC) $(CFLAGS) -o testString testString.o $(LDFLAGS)

testString.o: testString.c
	$(CC) $(CFLAGS) -c testString.c

clean:
	rm -rf test *.o *.dSYM
	rm -rf testString *.o *.dSYM