# Output binary
TARGET = htproxy

# Directories
SRC_DIR   = src
UTILS_DIR = $(SRC_DIR)/utils
ARGS_DIR  = $(UTILS_DIR)/args

# Object files
OBJS = $(SRC_DIR)/main.o \
       $(ARGS_DIR)/args.o

# Compiler
CC = gcc
CFLAGS = -Wall -Wextra -std=c11

# Pattern rule for object files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Final binary
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

.PHONY: clean format

clean:
	rm -f $(TARGET) $(SRC_DIR)/*.o $(ARGS_DIR)/*.o

# Compile main.c
$(SRC_DIR)/main.o: $(SRC_DIR)/main.c $(ARGS_DIR)/arg.h
	$(CC) $(CFLAGS) -c $< -o $@ -I$(ARGS_DIR)

# Compile args.c
$(ARGS_DIR)/args.o: $(ARGS_DIR)/args.c $(ARGS_DIR)/arg.h
	$(CC) $(CFLAGS) -c $< -o $@ -I$(ARGS_DIR)

# Format all C and header files recursively
format:
	find . -name "*.c" -o -name "*.h" | xargs clang-format -style=file -i
