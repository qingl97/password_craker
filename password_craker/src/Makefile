CFLAG= -g -Wall -Wextra  
ARGS=./passCrack -m "zzz" -a 26 -r 5 -t 3 -p 2

all: compile run 

compile: macro.h passCrack.c slave.c convert.c convert.h string_util.h
	mpicc passCrack.c convert.c string_util.c mpi_util.c -o passCrack $(CFLAG)
	mpicc slave.c convert.c mpi_util.c string_util.c -o  slave -fopenmp -pthread -g $(CFLAG) 

valgrind:
	mpiexec -np 1 valgrind $(ARGS)

run:
	mpiexec -np 1 $(ARGS)

clean: 
	rm slave passCrack
