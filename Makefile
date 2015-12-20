CC=gcc
NUM_TESTS=1


build:
	$(foreach var,$(NUM_TESTS),$(CC) test$(var).c fs.c -o test$(var) -O -Wall;)


debug:
	$(foreach var,$(NUM_TESTS),$(CC) test$(var).c fs.c -o test$(var) -O -Wall -g;)

clean:
	rm -f main
	rm -f *.o
	$(foreach var,$(NUM_TESTS),rm -f test$(var);)
