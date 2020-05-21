SOURCE = $(wildcard *.c)
TARGETS = $(patsubst %.c, %, $(SOURCE))

CC = gcc
CFLAGS = -Wall -g -lpthread

all:$(TARGETS)
$(TARGETS):%:%.c
	$(CC) $< $(CFLAGS) -o $@

.PHONY:clean all
clean:
	rm -rf $(TARGETS) cscope.* tags a.out 
