CFLAG= -g -Wall -Wextra  
ARGS=./craker letters 5 10 6 KPMG

all: compile run 

compile: mpi_utils.h master.c slave.c wordHandler.h
	mpicc master.c wordHandler.c -o craker $(CFLAG)
	mpicc slave.c wordHandler.c -o slave -fopenmp -g $(CFLAG) 

valgrind:
	mpirun -np 1 valgrind $(ARGS)

run:
	mpirun -np 1 $(ARGS)

clean: 
	rm slave craker
