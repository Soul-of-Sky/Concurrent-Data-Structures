CC = gcc
CFLAGS = -g -O2 -I ../include -I ../reclamation
LDFLAGS = -lpthread
RM = rm -f

all: blocking_queue_test lock_free_queue_test

blocking_queue_test: blocking/queue.c blocking/test.c
	$(CC) $^ $(CFLAGS) -I blocking $(LDFLAGS) -o $@

lock_free_queue_test: lock_free/queue.c lock_free/test.c ../reclamation/ebr.c
	$(CC) $^ $(CFLAGS) -I lock_free $(LDFLAGS) -o $@

clean:
	$(RM) blocking_queue_test 
	$(RM) lock_free_queue_test
	$(RM) *.o
