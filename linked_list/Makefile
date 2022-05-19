CC = gcc
CFLAGS = -g -O2 -I ../include -I ../reclamation
LDFLAGS = -lpthread
RM = rm -f

all: lazy_sync_linked_list_test lock_free_linked_list_test

lazy_sync_linked_list_test: lazy_sync/linked_list.c test.c ../reclamation/ebr.c
	$(CC) $^ $(CFLAGS) -I lazy_sync $(LDFLAGS) -o $@

lock_free_linked_list_test: lock_free/linked_list.c test.c ../reclamation/ebr.c
	$(CC) $^ $(CFLAGS) -I lock_free $(LDFLAGS) -o $@

clean:
	$(RM) lazy_sync_linked_list_test
	$(RM) lock_free_linked_list_test
	$(RM) *.o
