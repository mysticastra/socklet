CC = gcc
CFLAGS = -Wall -Wextra

SRCDIR = examples
INCDIR = .
OBJDIR = obj

TARGET = bin/socklet_example
SRC = $(SRCDIR)/main.c
OBJ = $(OBJDIR)/main.o

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $(TARGET)  -lssl -lcrypto

$(OBJDIR):
	mkdir -p $(OBJDIR)

$(OBJDIR)/%.o: $(SRCDIR)/%.c $(INCDIR)/socklet.h | $(OBJDIR)
	$(CC) $(CFLAGS) -I$(INCDIR) -c $< -o $@

clean:
	rm -rf $(OBJDIR) $(TARGET)

run: $(TARGET)
	./$(TARGET)

help:
	@echo "Available targets:"
	@echo "  all       - Build the executable"
	@echo "  clean     - Remove object files and executable"
	@echo "  run       - Run the program"
	@echo "  help      - Show this help message"
