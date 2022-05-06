CC = gcc
CFLAGS = -g -O2 -I ../include
LDFLAGS = -lpthread
RM = rm -f

all: concurrent_heap_test

concurrent_heap_test: heap.c test.c
	$(CC) $^ $(CFLAGS) $(LDFLAGS) -o $@

clean:
	$(RM) concurrent_heap_test
	$(RM) *.o
