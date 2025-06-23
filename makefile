CC = gcc
CFLAGS = -Wall -pedantic -std=c11
DEBUGFLAG = -g $(CFLAGS)
LIBS = -lpthread
OBJS = main.o parser/parser_rescuers.o parser/parser_emergency_types.o parser/parser_env.o Utils/Funzioni.o Utils/Lista.o thrd_Functions/rescuer_on_scene.o \
		thrd_Functions/rescuers_return.o thrd_Functions/thrd_insert.o thrd_Functions/thrd_operatori.o
OBJCLIENT = client.o
LOGFILE = logFile.txt

.PHONY: default clear run valgrind gdb

default: release debug client

%.o: %.c %.h
	$(CC) -c $(CFLAGS) $< -o $@ 

release: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

debug: $(OBJS)
	$(CC) $(DEBUGFLAG) -o $@ $^ $(LIBS)

client: $(OBJCLIENT)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

run: release
	@if [ -f $(LOGFILE) ]; then \
		rm $(LOGFILE); \
	fi
	./release

valgrind_release: release
	@if [ -f $(LOGFILE) ]; then \
		rm $(LOGFILE); \
	fi
	valgrind --leak-check=full ./release

valgrind_debug: debug
	@if [ -f $(LOGFILE) ]; then \
		rm $(LOGFILE); \
	fi
	valgrind --leak-check=full ./debug

gdb: debug
	@if [ -f $(LOGFILE) ]; then \
		rm $(LOGFILE); \
	fi
	gdb ./debug

clear: 
	rm -f release debug $(OBJS) $(OBJCLIENT) client logFile.txt
