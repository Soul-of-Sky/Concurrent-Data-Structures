CC = gcc
CFLAGS = -g -O0 -I ../include
LDFLAGS = -lpthread
RM = rm

all: lazy_test

lazy_test: lazy/linked_list.c lazy/test.c
	$(CC) $^ $(CFLAGS) $(LDFLAGS) -o $@

clean:
	$(RM) lazy_test
	$(RM) *.o
