
CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -pthread
LDFLAGS = -pthread

TARGET = vm
SOURCES = main.c
HEADERS = vm.h

all: $(TARGET)

$(TARGET): $(SOURCES) $(HEADERS)
	$(CC) $(CFLAGS) $(SOURCES) -o $(TARGET) $(LDFLAGS)

clean:
	rm -f $(TARGET)

.PHONY: all clean
