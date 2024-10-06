include libh8300h.mk

CC = gcc
CFLAGS = -Wall -g -std=c89
TARGET = libh8300h-tests
SOURCES = $(H8_SOURCES) main.c
HEADERS = $(H8_HEADERS)

all: $(TARGET)

$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCES)

run: $(TARGET)
	./$(TARGET)
	@if [ $$? -eq 0 ]; then \
		echo "All tests passed!"; \
	else \
		echo "Tests failed..."; \
		exit 1; \
	fi

clean:
	rm -f $(TARGET) *.o

.PHONY: clean run
