TARGETS=database.o main.o serial.o service.o protocol.o defs.o disk.o
INC=-I . -lsqlite3 -lm
OPT=-O0
DEBUG=-g

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