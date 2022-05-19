CC = gcc
CFLAGS = -g -O2 -I ../include -I ../reclamation
LDFLAGS = -lpthread
RM = rm -f

all: lock_free_test

lock_free_test: lock_free/hashset.c lock_free/linked_list.c test.c ../reclamation/ebr.c
	$(CC) $^ $(CFLAGS) -I lock_free $(LDFLAGS) -o $@

clean:
	$(RM) lock_free_test
	$(RM) cuckoo_test 
	$(RM) *.o
