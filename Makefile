CC=icc
INCLUDE_DIR=src/include
ODIR=obj
SOURCE=$(wildcard src/*.c)
CFLAGS=-c -Wall -Wextra -openmp -I$(INCLUDE_DIR)
LDFLAGS=
OBJECTS=$(SOURCE_DIR:.c=ODIR:.o)
LIBS=
LDIR=

EXEC=craker

all: $(EXEC)

$(EXEC): $(OBJECTS)
	$(CC) $(LD_FLAGS)  $(OBJECTS)

$(ODIR)/%.o: %.c
	$(CC) $(CFLAGS) $< -o $@

.PHONY: clean

clean:
	rm *.o $(ODIR)/*.o *~ craker
