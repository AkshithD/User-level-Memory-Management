CC = gcc
CFLAGS = -g -c -m32
LDFLAGS = -lm
AR = ar -rc
RANLIB = ranlib

all: clean my_vm.a

my_vm.o: my_vm.c my_vm.h
	$(CC) $(CFLAGS) my_vm.c $(LDFLAGS)

my_vm.a: my_vm.o
	$(AR) $@ $^
	$(RANLIB) $@

clean:
	rm -rf *.o *.a
