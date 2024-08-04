CC := gcc
CFLAGS := -Wall -Wextra

SRC := $(wildcard *.c)
OBJ := $(SRC:.c=.o)

TARGET := todo

PREFIX = /usr/local
BINDIR = $(PREFIX)/bin
LIBDIR = $(PREFIX)/lib
INCLUDEDIR = $(PREFIX)/include

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) $^ -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

install: $(TARGET)
	install -m 755 $(TARGET) $(BINDIR)
	@echo "Installation complete."	

uninstall:
	rm -f $(BINDIR)/$(TARGET)
	@echo "Uninstallation complete."

clean:
	rm -f $(OBJ) $(TARGET)

.PHONY: clean install
