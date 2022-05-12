CC = gcc
CFLAGS = -g -O0 -I ../include
LDFLAGS = -lpthread
RM = rm -f

all: reclamation_test libreclamation.a

reclamation_test: ebr.o qsbr.o hpbr.o test.o
	$(CC) $^ $(CFLAGS) $(LDFLAGS) -o $@

%.o: %.c
	$(CC) -c $^ $(CFLAGS) $(LDFLAGS) -o $@

clean:
	$(RM) reclamation_test
	$(RM) *.o
