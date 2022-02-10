CC = gcc
CFLAGS = -g -O0
LDFLAGS = -lpthread

RM = del

all: kv

kv: kv.c
	$(CC) $(CFLAGS) $^ $(LDFLAGS) -o $@

clean: 
	$(RM) kv.exe