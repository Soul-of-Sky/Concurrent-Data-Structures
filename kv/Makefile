CC = gcc
CFLAGS = -g -O0
LDFLAGS = -lpthread

RM = rm

all: kv

kv: kv.c
	$(CC) $(CFLAGS) $^ $(LDFLAGS) -o $@

clean: 
	$(RM) kv