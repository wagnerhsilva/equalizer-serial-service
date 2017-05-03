TARGETS=database.c main.c serial.c service.c protocol.c defs.c
OUTPUT=service
INC=-I . -lsqlite3
OPT=-O2
DEBUG=-g
all:
	${CC} $(OPT) $(TARGETS) -o $(OUTPUT) $(INC) $(DEBUG)
clean:
	rm -rf *o service
