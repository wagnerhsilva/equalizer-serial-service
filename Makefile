TARGETS=database.o main.o serial.o service.o protocol.o defs.o disk.o
INC=-I . -lsqlite3 -lm
OPT=-O2
DEBUG=-g

.PHONY: all clean rebuild

all: service

rebuild: clean service

%.o: %.c
	$(CC) $(INC) $(OPT) $(DEBUG) -c $^

service: $(TARGETS)
	$(CC) $(OPT) $^ -o service $(INC) $(DEBUG)

clean:
	rm -rf *o service
