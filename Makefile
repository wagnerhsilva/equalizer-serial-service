TARGETS=database.c main.c serial.c service.c protocol.c defs.c
OUTPUT=service
INC=-I . -lsqlite3
OPT=-O2
all:
	gcc $(OPT) $(TARGETS) -o $(OUTPUT) $(INC)
