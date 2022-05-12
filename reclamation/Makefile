CC = gcc
CFLAGS = -g -O0 -I ../include
LDFLAGS = -lpthread
RM = rm -f

all: reclamation_test

reclamation_test: ebr.c qsbr.c hpbr.c test.c
	$(CC) $^ $(CFLAGS) $(LDFLAGS) -o $@

clean:
	$(RM) reclamation_test
	$(RM) *.o
