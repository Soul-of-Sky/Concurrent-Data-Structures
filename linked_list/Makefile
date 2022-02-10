CC = gcc
CFLAGS = -g -O0 -I ../inc
LDFLAGS = -lpthread
RM = rm

all: linked_list

linked_list: linked_list.c
	$(CC) $^ $(CFLAGS) $(LDFLAGS) -o $@

clean:
	$(RM) linked_list
	$(RM) *.o
