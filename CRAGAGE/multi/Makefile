all: craquage_multithread esclave

craquage_multithread: craquage_multithread.o
	gcc craquage_multithread.o -o craquage_multithread -lpvm3 -lpthread
esclave: esclave.o
	gcc esclave.o -o esclave -lpvm3 -lpthread
craquage.o: craquage_multithread.c intervalle.h
	gcc -c craquage_multithread.c
esclave.o: esclave.c intervalle.h
	gcc -c esclave.c

clean:
	rm -f *.o

