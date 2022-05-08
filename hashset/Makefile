CC = gcc
CFLAGS = -g -O0 -I ../include
LDFLAGS = -lpthread
RM = rm -f

all: lock_free_test #cuckoo_test



lock_free_test: lock_free/hashset.c lock_free/linked_list.c test.c
	$(CC) $^ $(CFLAGS) -I lock_free $(LDFLAGS) -o $@

cuckoo_test: cuckoo/hashset.c test.c
	$(CC) $^ $(CFLAGS) -I cuckoo $(LDFLAGS) -o $@

clean:
	$(RM) lock_free_test
	$(RM) cuckoo_test 
	$(RM) *.o
