# Patrick Carra
# make is used to compile binary
# make clean is used to remove all binaries and object files
#
# T1: T1.c
# 	gcc -o T1 T1.c



CC = gcc
CFLAGS = -Wall
objects = runsim testsim
all: $(objects)

$(objects): %: %.c
	$(CC) $(CFLAGS) $< -L. -llogfile -llicenseobj -o $@

clean:
	rm *.o $(objects)
