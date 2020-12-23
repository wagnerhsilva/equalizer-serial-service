TARGETS= \
	database.o \
	main.o \
	serial.o \
	service.o \
	protocol.o \
	defs.o \
	disk.o \
	component.o \
	manager.o \
	bits.o \

INC=-I . -L/usr/lib/gcc-cross/arm-linux-gnueabihf/5/lib -lsqlite3 -lm -lpthread -lrt 
OPT=-O0
DEBUG=-g
CC = arm-linux-gnueabihf-gcc

.PHONY: all clean rebuild release

all: service

release: rebuild clean_partial

rebuild: clean service

%.o: %.c
	$(CC) $(INC) $(OPT) $(DEBUG) -c $^

service: $(TARGETS)
	$(CC) $(OPT) $^ -o service $(INC) $(DEBUG)

clean:
	rm -rf *o *.txt service

clean_partial:
	rm -rf *o *.txt