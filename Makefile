# Compiler
CC = gcc

# Compiler Flags
CFLAGS = -Wall -Wextra -g -I./src/include

# Directories
SRCDIR = src/core
INCLUDEDIR = src/include
BUILDDIR = build
BINDIR = bin

# Source Files
SOURCES = $(SRCDIR)/event.c $(SRCDIR)/base64.c $(SRCDIR)/frame.c $(SRCDIR)/websocket.c $(SRCDIR)/client.c $(SRCDIR)/server.c
OBJECTS = $(SOURCES:$(SRCDIR)/%.c=$(BUILDDIR)/%.o)

# Executable
TARGET = $(BINDIR)/socklet

# Default rule
all: $(TARGET)

# Build executable
$(TARGET): $(OBJECTS) app/main.c
	@mkdir -p $(BINDIR)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJECTS) app/main.c -lssl -lcrypto

# Compile source files
$(BUILDDIR)/%.o: $(SRCDIR)/%.c $(INCLUDEDIR)/%.h
	@mkdir -p $(BUILDDIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Clean up object files and executable
clean:
	rm -rf $(BUILDDIR) $(BINDIR)

# Run the server
run: $(TARGET)
	./$(TARGET)

# Test configuration
test-config:
	cat socklet.config
