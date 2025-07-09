CC = gcc
CFLAGS = -O3 -Wall -pedantic -std=c11
LIBS = -lpthread
NAME = main
OBJS = $(NAME).o parser/parser_rescuers.o parser/parser_emergency_types.o parser/parser_env.o Utils/Funzioni.o Utils/Lista.o thrd_Functions/rescuer_on_scene.o \
		thrd_Functions/rescuers_return.o thrd_Functions/thrd_insert.o thrd_Functions/thrd_operatori.o
OBJCLIENT = client.o
LOGFILE = logFile.txt

.PHONY: default clear run valgrind

default: $(NAME) client

%.o: %.c %.h
	$(CC) -c $(CFLAGS) $< -o $@ 

$(NAME): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

client: $(OBJCLIENT)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

run: $(NAME)
	@if [ -f $(LOGFILE) ]; then \
		rm $(LOGFILE); \
	fi
	./$(NAME)

valgrind: $(NAME)
	@if [ -f $(LOGFILE) ]; then \
		rm $(LOGFILE); \
	fi
	valgrind --leak-check=full ./$(NAME)

clear: 
	rm -f $(NAME) $(OBJS) $(OBJCLIENT) client logFile.txt
