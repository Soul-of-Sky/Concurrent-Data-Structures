CC = gcc
CFLAGS = -g -O0 -I ../include
LDFLAGS = -lpthread
RM = rm -f

all: bptree_test

bptree_test: bptree.c test.c
	$(CC) $^ $(CFLAGS) $(LDFLAGS) -o $@

clean:
	$(RM) bptree_test
	$(RM) *.o
