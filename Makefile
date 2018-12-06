CC=gcc
CFLAGS= -Wall -g
H=cpu.h print.h
OBJ=main.o cpu.o parse.o print.o
LIBS=

%.o: %.c $(H)
	$(CC) $(CFLAGS) -c -o $@ $< 

sim: $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ 

.PHONY: clean

clean:
	rm -f $(OBJ) sim
