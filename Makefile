CC=gcc
NUM_TESTS=1


build: fs tests


tests:
	$(foreach var,$(NUM_TESTS),$(CC) test$(var).c fs.o -o test$(var) -O;)


fs:
	$(CC) -c fs.c -o fs.o -O2



clean:
	rm -f main
	rm -f *.o
