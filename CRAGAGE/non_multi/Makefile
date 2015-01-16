all: craquage esclave

craquage: craquage.o
	gcc craquage.o -o craquage -lpvm3 -lpthread
esclave: esclave.o
	gcc esclave.o -o esclave -lpvm3
craquage.o: craquage.c intervalle.h
	gcc -c craquage.c
esclave.o: esclave.c intervalle.h
	gcc -c esclave.c

clean:
	rm -f *.o

