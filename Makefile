# Output binary
TARGET = htproxy

# Directories
SRC_DIR   = src
UTILS_DIR = $(SRC_DIR)/utils
HTTP_DIR  = $(SRC_DIR)/http
CACHE_DIR = $(SRC_DIR)/cache
SOCKET_DIR = $(SRC_DIR)/socket
PROXY_DIR = $(SRC_DIR)/proxy

# Object files
OBJS = $(SRC_DIR)/main.o \
       $(UTILS_DIR)/utils.o \
       $(HTTP_DIR)/http.o \
       $(CACHE_DIR)/cache.o \
       $(SOCKET_DIR)/socket.o \
       $(PROXY_DIR)/proxy.o

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
	rm -f $(TARGET) $(SRC_DIR)/*.o $(UTILS_DIR)/*.o $(HTTP_DIR)/*.o $(CACHE_DIR)/*.o $(SOCKET_DIR)/*.o $(PROXY_DIR)/*.o

# Compile main.c
$(SRC_DIR)/main.o: $(SRC_DIR)/main.c $(UTILS_DIR)/utils.h
	$(CC) $(CFLAGS) -c $< -o $@ -I$(UTILS_DIR)

# Compile utils.c
$(UTILS_DIR)/utils.o: $(UTILS_DIR)/utils.c $(UTILS_DIR)/utils.h
	$(CC) $(CFLAGS) -c $< -o $@ -I$(UTILS_DIR)

# Compile http.c
$(HTTP_DIR)/http.o: $(HTTP_DIR)/http.c $(HTTP_DIR)/http.h $(UTILS_DIR)/utils.h
	$(CC) $(CFLAGS) -c $< -o $@ -I$(HTTP_DIR) -I$(UTILS_DIR)

# Compile cache.c
$(CACHE_DIR)/cache.o: $(CACHE_DIR)/cache.c $(CACHE_DIR)/cache.h $(UTILS_DIR)/utils.h
	$(CC) $(CFLAGS) -c $< -o $@ -I$(CACHE_DIR) -I$(UTILS_DIR)

# Compile socket.c
$(SOCKET_DIR)/socket.o: $(SOCKET_DIR)/socket.c $(SOCKET_DIR)/socket.h $(UTILS_DIR)/utils.h
	$(CC) $(CFLAGS) -c $< -o $@ -I$(SOCKET_DIR) -I$(UTILS_DIR)

# Compile proxy.c
$(PROXY_DIR)/proxy.o: $(PROXY_DIR)/proxy.c $(PROXY_DIR)/proxy.h $(HTTP_DIR)/http.h $(CACHE_DIR)/cache.h $(SOCKET_DIR)/socket.h
	$(CC) $(CFLAGS) -c $< -o $@ -I$(PROXY_DIR) -I$(HTTP_DIR) -I$(CACHE_DIR) -I$(SOCKET_DIR)

# Format all C and header files recursively
format:
	find . -name "*.c" -o -name "*.h" | xargs clang-format -style=file -i
