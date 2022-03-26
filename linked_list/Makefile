CC = gcc
CFLAGS = -g -O0 -I ../include
LDFLAGS = -lpthread
RM = rm

all: linked_list

linked_list: linked_list.c test.c
	$(CC) $^ $(CFLAGS) $(LDFLAGS) -o $@

clean:
	$(RM) linked_list
	$(RM) *.o
