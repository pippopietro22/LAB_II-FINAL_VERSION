CC = gcc
CFLAGS = -O3 -Wall -pedantic -std=c11
LIBS = -lpthread
OBJS = main.o parser/parser_rescuers.o parser/parser_emergency_types.o parser/parser_env.o Utils/Funzioni.o Utils/Lista.o thrd_Functions/rescuer_on_scene.o \
		thrd_Functions/rescuers_return.o thrd_Functions/thrd_insert.o thrd_Functions/thrd_operatori.o
OBJCLIENT = client.o
LOGFILE = logFile.txt

.PHONY: default clear run valgrind gdb

default: release client

%.o: %.c %.h
	$(CC) -c $(CFLAGS) $< -o $@ 

release: $(OBJS)
	$(CC) $(DEBUGFLAG) -o $@ $^ $(LIBS)

client: $(OBJCLIENT)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

run: release
	@if [ -f $(LOGFILE) ]; then \
		rm $(LOGFILE); \
	fi
	./release

valgrind: release
	@if [ -f $(LOGFILE) ]; then \
		rm $(LOGFILE); \
	fi
	valgrind --leak-check=full ./release

gdb: release
	@if [ -f $(LOGFILE) ]; then \
		rm $(LOGFILE); \
	fi
	gdb ./release

clear: 
	rm -f release $(OBJS) $(OBJCLIENT) client logFile.txt
