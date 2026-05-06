CC = gcc
MODULE_DIRS = $(wildcard */)
CFLAGS = -I. $(addprefix -I, $(MODULE_DIRS)) -pthread
SRCS = $(wildcard *.c) $(wildcard */*.c)
OBJS = $(SRCS:.c=.o)
TARGET = main

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

.PHONY: clean
clean:
	rm -f $(OBJS) $(TARGET)