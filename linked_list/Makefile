CC = gcc
CFLAGS = -g -O2 -I ../include
LDFLAGS = -lpthread
RM = rm

all: lazy_sync_test lock_free_test

lazy_sync_test: lazy_sync/linked_list.c test.c
	$(CC) $^ $(CFLAGS) -I lazy_sync $(LDFLAGS) -o $@

lock_free_test: lock_free/linked_list.c test.c
	$(CC) $^ $(CFLAGS) -I lock_free $(LDFLAGS) -o $@

clean:
	$(RM) lazy_sync_test 
	$(RM) lock_free_test
	$(RM) *.o
