#define _GNU_SOURCE           // Enable GNU extensions 
#define _POSIX_C_SOURCE 200112L  // Enable POSIX getaddrinfo functions

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include "utils/args/arg.h"

#define BUFFER_SIZE 4096
#define MAX_HEADERS 50
#define MAX_HEADER_SIZE 1024

#define MAX_REQUEST_SIZE 2000
#define MAX_RESPONSE_SIZE 102400
#define CACHE_SIZE 10

int create_listening_socket(int port);
int read_http_headers(int socket, char ***headers, int *header_count);
void parse_request_line(const char *line, char *method, char *uri, char *version);
char* find_host_header(char **headers, int header_count);
int handle_client_request(int client_socket);
int connect_to_server(const char *hostname);
int forward_response(int server_socket, int client_socket, const char *request, int request_len, const char *hostname, const char *uri);
void free_headers(char **headers, int header_count);


typedef struct cache_entry {
    char request[MAX_REQUEST_SIZE];
    char response[MAX_RESPONSE_SIZE];
    int response_size;         
    char host[256];           
    char uri[256];            
    int valid;                

    struct cache_entry *prev; 
    struct cache_entry *next;

} cache_entry;

// LRU Cache structure
typedef struct {
    cache_entry entries[CACHE_SIZE];  
    cache_entry *head;                
    cache_entry *tail;                
    int count;                        
} lru_cache;

// Global cache instance
lru_cache cache;

// Function declarations for cache operations
void init_cache();
void add_to_cache(const char *request, int request_len, const char *response, int response_len, const char *host, const char *uri);
cache_entry* find_in_cache(const char *request, int request_len);
void move_to_front(cache_entry *entry);
cache_entry* evict_lru();
void build_request_string(char **headers, int header_count, char *request_buffer, int *request_len);


/**
 * @brief Main function. 
 *
 * @param argc Argument count
 * @param argv Argument vector
 * @return int Exit status
 */
int main(int argc, char *argv[]) {
    int port;
    int c_flag;

    // Parse command-line arguments
    parse_args(argc, argv, &port, &c_flag);

    // Initialise the cache
    init_cache();

    // Create a listening socket
    int listen_socket = create_listening_socket(port);
    if (listen_socket < 0) {
        fprintf(stderr, "Failed to create listening socket\n");
        return 1;
    }

    // Start listening (With backlog of 10)
    if (listen(listen_socket, 10) < 0) {
        perror("listen failed");
        close(listen_socket);
        return 1;
    }

    // Main server loop
    while (1) {
        struct sockaddr_storage client_addr;
        socklen_t client_len = sizeof(client_addr);
    
        int client_socket = accept(listen_socket, (struct sockaddr*)&client_addr, &client_len);
        if (client_socket < 0) {
            perror("accept failed");
            continue;
        }
    
        printf("Accepted\n");
        fflush(stdout);
    
        // Handle HTTP request
        if (handle_client_request(client_socket) < 0) {
            fprintf(stderr, "Failed to handle client request\n");
        }

        close(client_socket);
    }

    close(listen_socket);
    printf("shutdown complete");
    return 0;
}

/**
 * @brief Create dual-stack TCP listening socket (accepts both IPv4 and IPv6)
 * 
 * @param port Port number
 * @return int Socket file descriptor
 */
int create_listening_socket(int port) {
    char service[16];
    int re, s, sockfd;
    struct addrinfo hints, *res;
    
    // Convert port to string
    snprintf(service, sizeof(service), "%d", port);

    // Create address we're going to listen on (with given port number)
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET6;      // IPv6 (will also handle IPv4 with dual-stack)
    hints.ai_socktype = SOCK_STREAM; // TCP
    hints.ai_flags = AI_PASSIVE;     // For bind
    
    s = getaddrinfo(NULL, service, &hints, &res);
    if (s != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
        exit(EXIT_FAILURE);
    }

    // Create IPv6 socket
    sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sockfd < 0) {
        perror("socket");
        freeaddrinfo(res);
        exit(EXIT_FAILURE);
    }

    // Reuse port if possible
    re = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &re, sizeof(int)) < 0) {
        perror("setsockopt SO_REUSEADDR");
        freeaddrinfo(res);
        exit(EXIT_FAILURE);
    }
    
    // Disable IPv6-only mode to accept IPv4 connections too
    int ipv6_only = 0;
    if (setsockopt(sockfd, IPPROTO_IPV6, IPV6_V6ONLY, &ipv6_only, sizeof(ipv6_only)) < 0) {
        perror("setsockopt IPV6_V6ONLY");
        freeaddrinfo(res);
        exit(EXIT_FAILURE);
    }
    
    // Bind address to the socket
    if (bind(sockfd, res->ai_addr, res->ai_addrlen) < 0) {
        perror("bind");
        freeaddrinfo(res);
        exit(EXIT_FAILURE);
    }
    
    freeaddrinfo(res);
    return sockfd;
}

/**
 * @brief Read HTTP headers from socket with dynamic allocation
 * 
 * @param socket Socket to read from
 * @param headers Array to store header lines
 * @return int Number of headers read, or -1 on error
 */
int read_http_headers(int socket, char ***headers, int *header_count) {
    char *read_buffer = malloc(BUFFER_SIZE);
    if (!read_buffer) {
        fprintf(stderr, "Failed to allocate read buffer\n");
        return -1;
    }
    
    *headers = malloc(MAX_HEADERS * sizeof(char*));
    if (!*headers) {
        fprintf(stderr, "Failed to allocate headers array\n");
        free(read_buffer);
        return -1;
    }
    
    *header_count = 0;
    int total_received = 0;
    int buffer_capacity = BUFFER_SIZE - 1;
    
    while (*header_count < MAX_HEADERS) {
        // Read more data
        int space_left = buffer_capacity - total_received;
        if (space_left <= 1) {
            // Need more space in read buffer
            buffer_capacity *= 2;
            read_buffer = realloc(read_buffer, buffer_capacity + 1);
            if (!read_buffer) {
                fprintf(stderr, "Failed to expand read buffer\n");
                free_headers(*headers, *header_count);
                return -1;
            }
            space_left = buffer_capacity - total_received;
        }
        
        int bytes = recv(socket, read_buffer + total_received, space_left, 0);
        if (bytes <= 0) {
            free(read_buffer);
            free_headers(*headers, *header_count);
            return -1;
        }
        
        total_received += bytes;
        read_buffer[total_received] = '\0';
        
        // Look for complete lines ending with \r\n
        char *line_start = read_buffer;
        char *line_end;
        
        while ((line_end = strstr(line_start, "\r\n")) != NULL) {
            *line_end = '\0';  // Terminate the line
            
            // Check for blank line (end of headers)
            if (strlen(line_start) == 0) {
                free(read_buffer);
                return 0;  // Success
            }
            
            // Allocate memory for this header line
            int line_len = strlen(line_start);
            (*headers)[*header_count] = malloc(line_len + 1);
            if (!(*headers)[*header_count]) {
                fprintf(stderr, "Failed to allocate memory for header\n");
                free(read_buffer);
                free_headers(*headers, *header_count);
                return -1;
            }
            
            // Copy the header
            strcpy((*headers)[*header_count], line_start);
            (*header_count)++;
            
            if (*header_count >= MAX_HEADERS) {
                free(read_buffer);
                return 0;  // Hit max headers limit
            }
            
            // Move to next line
            line_start = line_end + 2;  // Skip \r\n
        }
        
        // Move incomplete line to beginning of buffer
        int remaining = strlen(line_start);
        if (remaining > 0 && line_start != read_buffer) {
            memmove(read_buffer, line_start, remaining);
            total_received = remaining;
        } else if (remaining == 0) {
            total_received = 0;
        }
    }
    
    free(read_buffer);
    return 0;
}

/**
 * @brief Free dynamically allocated headers
 * 
 * @param headers Array of header strings
 * @param header_count Number of headers to free
 */
void free_headers(char **headers, int header_count) {
    if (headers) {
        for (int i = 0; i < header_count; i++) {
            free(headers[i]);
        }
        free(headers);
    }
}

/**
 * @brief Parse HTTP request line
 * 
 * @param line Request line to parse
 * @param method Buffer to store HTTP method
 * @param uri Buffer to store URI
 * @param version Buffer to store HTTP version
 */
void parse_request_line(const char *line, char *method, char *uri, char *version) {
    sscanf(line, "%s %s %s", method, uri, version);
}

/**
 * @brief Find Host header in headers array
 * 
 * @param headers Array of header strings
 * @param header_count Number of headers
 * @return char* Pointer to hostname, or NULL if not found
 */
char* find_host_header(char **headers, int header_count) {
    for (int i = 0; i < header_count; i++) {
        if (strncasecmp(headers[i], "Host:", 5) == 0) {
            // Skip "Host: " and return hostname
            return headers[i] + 6;  // Skip "Host: "
        }
    }
    return NULL;
}

/**
 * @brief Connect to origin server
 * 
 * @param hostname Hostname to connect to
 * @return int Socket file descriptor, or -1 on error
 */
int connect_to_server(const char *hostname) {
    struct addrinfo hints, *result, *rp;
    int sockfd;
    
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;    // Allow IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM;
    
    // Resolve hostname
    int status = getaddrinfo(hostname, "80", &hints, &result);
    if (status != 0) {
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
        return -1;
    }
    
    // Try each address until one works
    for (rp = result; rp != NULL; rp = rp->ai_next) {
        sockfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sockfd == -1) continue;
        
        if (connect(sockfd, rp->ai_addr, rp->ai_addrlen) != -1) {
            break; // Success
        }
        
        close(sockfd);
    }
    
    freeaddrinfo(result);
    
    if (rp == NULL) {
        fprintf(stderr, "Could not connect to %s\n", hostname);
        return -1;
    }
    
    return sockfd;
}

/**
 * @brief Forward response from server to client
 * 
 * @param server_socket Socket connected to origin server
 * @param client_socket Socket connected to client
 * @return int 0 on success, -1 on error
 */
int forward_response(int server_socket, int client_socket, const char *request, int request_len, const char *hostname, const char *uri) {
    char buffer[BUFFER_SIZE * 2]; //
    int content_length = -1;
    int headers_complete = 0;
    int total_received = 0;
    char header_buffer[BUFFER_SIZE * 4] = {0};
    
    // Read response headers first
    while (!headers_complete && total_received < (int)(sizeof(header_buffer) - 1)) {
        int bytes = recv(server_socket, buffer, 1, 0);
        if (bytes <= 0) return -1;
        
        header_buffer[total_received] = buffer[0];
        total_received++;
        
        // Check for end of headers (\r\n\r\n)
        if (total_received >= 4 && 
            strncmp(header_buffer + total_received - 4, "\r\n\r\n", 4) == 0) {
            headers_complete = 1;
            
            // Look for Content-Length in headers
            char *cl_pos = strstr(header_buffer, "Content-Length:");
            if (!cl_pos) {
                cl_pos = strstr(header_buffer, "content-length:");
            }
            if (cl_pos) {
                content_length = atoi(cl_pos + 15);
                printf("Response body length %d\n", content_length); 
                fflush(stdout);
            } else {
                printf("Response body length 0\n");
                fflush(stdout);
            }
        }
    }
    
    // Send headers to client
    if (send(client_socket, header_buffer, total_received, 0) < 0) {
        return -1;
    }
    
    // Prepare for possible caching
    char *response_buffer = NULL;
    int response_size = total_received;
    int cacheable = (request_len < MAX_REQUEST_SIZE && content_length >= 0 && content_length <= MAX_RESPONSE_SIZE);
    
    if (cacheable) {
        // Allocate buffer for the complete response
        response_buffer = malloc(MAX_RESPONSE_SIZE);
        if (response_buffer) {
            // Copy headers to response buffer
            memcpy(response_buffer, header_buffer, total_received);
        } else {
            cacheable = 0; 
        }
    }
    
    // Forward response body if Content-Length specified
    if (content_length > 0) {
        int remaining = content_length;
        while (remaining > 0) {
            int to_read = (remaining < BUFFER_SIZE) ? remaining : BUFFER_SIZE;
            int bytes = recv(server_socket, buffer, to_read, 0);
            if (bytes <= 0) break;
            
            if (send(client_socket, buffer, bytes, 0) < 0) {
                if (response_buffer) free(response_buffer);
                return -1;
            }
            
            // Add to response buffer if caching
            if (cacheable && response_buffer && response_size + bytes <= MAX_RESPONSE_SIZE) {
                memcpy(response_buffer + response_size, buffer, bytes);
                response_size += bytes;
            } else if (cacheable) {
                cacheable = 0; // Response too large to cache
                free(response_buffer);
                response_buffer = NULL;
            }
            
            remaining -= bytes;
        }
    } else if (content_length < 0) {
        // No Content-Length header, read until connection closes
        // We won't cache these responses
        cacheable = 0;
        if (response_buffer) {
            free(response_buffer);
            response_buffer = NULL;
        }
        
        int bytes;
        while ((bytes = recv(server_socket, buffer, BUFFER_SIZE, 0)) > 0) {
            if (send(client_socket, buffer, bytes, 0) < 0) {
                return -1;
            }
        }
    }
    
    // Add to cache if cacheable
    if (cacheable && response_buffer && request_len < MAX_REQUEST_SIZE && response_size <= MAX_RESPONSE_SIZE) {
        add_to_cache(request, request_len, response_buffer, response_size, hostname, uri);
    }
    
    if (response_buffer) {
        free(response_buffer);
    }
    
    return 0;
}

/**
 * @brief Handle client request 
 * 
 * @param client_socket Socket connected to client
 * @return int 0 on success, -1 on error
 */
int handle_client_request(int client_socket) {
    char **headers = NULL;
    int header_count = 0;
    char method[16], uri[256], version[16]; // REMOVE MAGIC NUMBERS TODO
    
    // Read HTTP headers
    if (read_http_headers(client_socket, &headers, &header_count) < 0) {
        fprintf(stderr, "Failed to read headers\n");
        return -1;
    }
    
    if (header_count == 0) {
        fprintf(stderr, "No headers received\n");
        free_headers(headers, header_count);
        return -1;
    }
    
    // Parse request line (first header)
    parse_request_line(headers[0], method, uri, version);

    // Do not cache non-GET methods
    if (strcasecmp(method, "GET") != 0) {
        goto proxy_request;
    }

    // Log request tail (last header line)
    if (header_count > 0) {
        printf("Request tail %s\n", headers[header_count - 1]);
        fflush(stdout);
    }
    
    // Find Host header
    char *hostname = find_host_header(headers, header_count);
    if (!hostname) {
        fprintf(stderr, "No Host header found\n");
        free_headers(headers, header_count);
        return -1;
    }
    
    // Build complete request string for cache lookup
    char request_buffer[MAX_REQUEST_SIZE];
    int request_len = 0;
    build_request_string(headers, header_count, request_buffer, &request_len);
    
    // Check if request is cacheable (less than 2000 bytes)
    if (request_len < MAX_REQUEST_SIZE) {
        // Look for request in cache
        cache_entry *entry = find_in_cache(request_buffer, request_len);
        
        if (entry) {
            // Cache hit - serve from cache
            printf("Serving %s %s from cache\n", entry->host, entry->uri);
            fflush(stdout);
            
            // Send cached response to client
            if (send(client_socket, entry->response, entry->response_size, 0) < 0) {
                free_headers(headers, header_count);
                return -1;
            }
            
            free_headers(headers, header_count);
            return 0;
        }
    }
    
    proxy_request:        
        // Log what we're forwarding
        printf("GETting %s %s\n", hostname, uri);
        fflush(stdout);
        
        // Connect to server and proxy the request
        int server_socket = connect_to_server(hostname);
        if (server_socket < 0) {
            fprintf(stderr, "Failed to connect to %s\n", hostname);
            free_headers(headers, header_count);
            return -1;
        }
        
        // Forward original request to server
        for (int i = 0; i < header_count; i++) {
            if (send(server_socket, headers[i], strlen(headers[i]), 0) < 0 ||
                send(server_socket, "\r\n", 2, 0) < 0) {
                close(server_socket);
                free_headers(headers, header_count);
                return -1;
            }
        }
        // Send final \r\n to end headers
        if (send(server_socket, "\r\n", 2, 0) < 0) {
            close(server_socket);
            free_headers(headers, header_count);
            return -1;
        }
        
        // Forward response back to client and possibly cache it
        int result = forward_response(server_socket, client_socket, request_buffer, request_len, hostname, uri);
        
        close(server_socket);
        free_headers(headers, header_count);
        return result;
}

/**
 * @brief Initialise the LRU cache
 */
void init_cache() {
    memset(&cache, 0, sizeof(cache));
    cache.head = NULL;
    cache.tail = NULL;
    cache.count = 0;
    
    // Mark all entries as invalid initially
    for (int i = 0; i < CACHE_SIZE; i++) {
        cache.entries[i].valid = 0;
    }
}

/**
 * @brief Build a complete request string from headers
 * 
 * @param headers Array of header strings
 * @param header_count Number of headers
 * @param request_buffer Buffer to store the complete request
 * @param request_len Pointer to store the length of the request
 */
void build_request_string(char **headers, int header_count, char *request_buffer, int *request_len) {
    *request_len = 0;
    
    // Concatenate all headers with CRLF
    for (int i = 0; i < header_count; i++) {
        int header_len = strlen(headers[i]);
        if (*request_len + header_len + 2 >= MAX_REQUEST_SIZE) {
            // Request would be too large to cache
            *request_len = MAX_REQUEST_SIZE;  // Mark as too large
            return;
        }
        
        memcpy(request_buffer + *request_len, headers[i], header_len);
        *request_len += header_len;
        
        memcpy(request_buffer + *request_len, "\r\n", 2);
        *request_len += 2;
    }
    
    // Add final CRLF to end headers section
    if (*request_len + 2 < MAX_REQUEST_SIZE) {
        memcpy(request_buffer + *request_len, "\r\n", 2);
        *request_len += 2;
    } else {
        *request_len = MAX_REQUEST_SIZE;  // Mark as too large
    }
}

/**
 * @brief Find a request in the cache
 * 
 * @param request Request string to look for
 * @param request_len Length of the request
 * @return cache_entry* Pointer to cache entry if found, NULL otherwise
 */
cache_entry* find_in_cache(const char *request, int request_len) {
    // Search through all valid cache entries
    for (int i = 0; i < CACHE_SIZE; i++) {
        if (cache.entries[i].valid && 
            memcmp(cache.entries[i].request, request, request_len) == 0) {
            // Found in cache, move to front (most recently used)
            move_to_front(&cache.entries[i]);
            return &cache.entries[i];
        }
    }
    
    return NULL;  // Not found
}

/**
 * @brief Move a cache entry to the front of the LRU list (most recently used)
 * 
 * @param entry Cache entry to move
 */
void move_to_front(cache_entry *entry) {
    if (entry == cache.head) {
        // Already at front
        return;
    }
    
    // Remove from current position
    if (entry->prev) {
        entry->prev->next = entry->next;
    }
    
    if (entry->next) {
        entry->next->prev = entry->prev;
    }
    
    if (entry == cache.tail) {
        cache.tail = entry->prev;
    }
    
    // Move to front
    entry->next = cache.head;
    entry->prev = NULL;
    
    if (cache.head) {
        cache.head->prev = entry;
    }
    
    cache.head = entry;
    
    if (!cache.tail) {
        cache.tail = entry;
    }
}

/**
 * @brief Evict the least recently used cache entry
 * 
 * @return cache_entry* Pointer to the evicted entry (now invalid)
 */
cache_entry* evict_lru() {
    if (!cache.tail) {
        // Cache is empty
        return &cache.entries[0];
    }
    
    cache_entry *to_evict = cache.tail;
    
    // Log eviction
    fprintf(stderr, "Evicting %s %s from cache\n", to_evict->host, to_evict->uri);
    fflush(stderr);
    
    // Update tail pointer
    cache.tail = to_evict->prev;
    
    if (cache.tail) {
        cache.tail->next = NULL;
    } else {
        // Cache is now empty
        cache.head = NULL;
    }
    
    to_evict->valid = 0;
    to_evict->prev = NULL;
    to_evict->next = NULL;
    
    cache.count--;
    
    return to_evict;
}

/**
 * @brief Add a new entry to the cache
 * 
 * @param request Request string
 * @param request_len Length of the request
 * @param response Response data
 * @param response_len Length of the response
 * @param host Hostname from the request
 * @param uri URI from the request
 */
void add_to_cache(const char *request, int request_len, const char *response, int response_len, const char *host, const char *uri) {
    cache_entry *entry = NULL;
    
    // Find an empty slot or evict LRU if full
    if (cache.count < CACHE_SIZE) {
        // Find an invalid entry
        for (int i = 0; i < CACHE_SIZE; i++) {
            if (!cache.entries[i].valid) {
                entry = &cache.entries[i];
                break;
            }
        }
    } else {
        // Cache is full, evict LRU
        entry = evict_lru();
    }
    
   
    // Copy request and response data
    memcpy(entry->request, request, request_len);
    memcpy(entry->response, response, response_len);
    entry->response_size = response_len;
    
    // Copy host and URI for logging
    strncpy(entry->host, host, sizeof(entry->host) - 1);
    entry->host[sizeof(entry->host) - 1] = '\0';
    
    strncpy(entry->uri, uri, sizeof(entry->uri) - 1);
    entry->uri[sizeof(entry->uri) - 1] = '\0';
    
    entry->valid = 1;
    
    // Add to front of LRU list
    entry->next = cache.head;
    entry->prev = NULL;
    
    if (cache.head) {
        cache.head->prev = entry;
    }
    
    cache.head = entry;
    
    if (!cache.tail) {
        cache.tail = entry;
    }
    
    cache.count++;

    move_to_front(entry);
}