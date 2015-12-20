CC=gcc
NUM_TESTS=1 2


build: fs tests


tests:
	$(foreach var,$(NUM_TESTS),$(CC) test$(var).c fs.o -o test$(var) -O -Wall;)

fs:
	$(CC) -c fs.c -o fs.o -O2 -Wall

clean:
	rm -f main
	rm -f *.o
	$(foreach var,$(NUM_TESTS),rm -f test$(var);)
