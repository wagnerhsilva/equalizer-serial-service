TARGETS=database.c main.c serial.c service.c protocol.c defs.c
OUTPUT=service
INC=-I . -lsqlite3
OPT=-O2
all:
	${CC} $(OPT) $(TARGETS) -o $(OUTPUT) $(INC)
clean:
	rm -rf *o service
