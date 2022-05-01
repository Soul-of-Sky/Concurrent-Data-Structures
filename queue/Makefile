CC = gcc
CFLAGS = -g -O2 -I ../include
LDFLAGS = -lpthread
RM = rm -f

all: blocking_test lock_free_test

blocking_test: blocking/queue.c test.c
	$(CC) $^ $(CFLAGS) -I blocking $(LDFLAGS) -o $@

lock_free_test: lock_free/queue.c test.c
	$(CC) $^ $(CFLAGS) -I lock_free $(LDFLAGS) -o $@

clean:
	$(RM) blocking_test 
	$(RM) lock_free_test
	$(RM) *.o
