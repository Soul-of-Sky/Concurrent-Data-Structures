CC = gcc
CFLAGS = -g -O2 -I ../include -I ../reclamation
LDFLAGS = -lpthread
RM = rm -f

all: backoff_stack_test elimination_backoff_stack_test

backoff_stack_test: stack.c test.c ../reclamation/ebr.c
	$(CC) $^ $(CFLAGS) $(LDFLAGS) -D BACKOFF -o $@

elimination_backoff_stack_test: stack.c test.c ../reclamation/ebr.c
	$(CC) $^ $(CFLAGS) $(LDFLAGS) -D ELIMINATION -o $@

clean:
	$(RM) backoff_stack_test
	$(RM) elimination_backoff_stack_test
	$(RM) *.o
