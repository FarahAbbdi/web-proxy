#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include <errno.h>
#include "utils/args/arg.h"

#define BUFFER_SIZE 4096
#define MAX_HEADERS 50
#define MAX_HEADER_SIZE 1024

// Global variable for graceful shutdown
volatile sig_atomic_t running = 1;

void signal_handler(int sig);
int create_listening_socket(int port);
int read_http_headers(int socket, char headers[][MAX_HEADER_SIZE]);
void parse_request_line(const char *line, char *method, char *uri, char *version);
char* find_host_header(char headers[][MAX_HEADER_SIZE], int header_count);
int handle_client_request(int client_socket);

/**
 * @brief Main function. 
 *
 * @param argc Argument count
 * @param argv Argument vector
 * @return int Exit status
 */
int main(int argc, char *argv[])
{
    int port;
    int c_flag;

    // Parse command-line arguments
    parse_args(argc, argv, &port, &c_flag);

    // For demonstration: print parsed values
    printf("Listen port: %d\n", port);
    printf("-c flag: %s\n", c_flag ? "set" : "not set");

    // TODO: Add proxy logic here (CONVERT TO FUNCTION LATER)

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
    printf("Proxy ready to accept connections...\n");

    // Main server loop
    while (running) {
        struct sockaddr_storage client_addr;
        socklen_t client_len = sizeof(client_addr);
    
        int client_socket = accept(listen_socket, (struct sockaddr*)&client_addr, &client_len);
        if (client_socket < 0) {
            if (running) perror("accept failed");
                continue;
        }
    
        printf("Accepted\n");
        fflush(stdout);
    
        // TODO:HANDLE HTTP REQUEST
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
 * @brief Signal handler for graceful shutdown
 */
void signal_handler(int sig) {
    if (sig == SIGINT) {
        printf("\nReceived SIGINT, shutting down...\n");
        running = 0;
    }
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
    memset(&hints, 0, sizeof hints);
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
    
    // CRITICAL: Disable IPv6-only mode to accept IPv4 connections too
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
 * @brief Read HTTP headers from socket
 * 
 * @param socket Socket to read from
 * @param headers Array to store header lines
 * @return int Number of headers read, or -1 on error
 */
int read_http_headers(int socket, char headers[][MAX_HEADER_SIZE]) {
    char buffer[BUFFER_SIZE];
    int header_count = 0;
    int total_received = 0;
    
    while (header_count < MAX_HEADERS - 1) {
        // Read one byte at a time to find line endings
        int bytes = recv(socket, buffer + total_received, 1, 0);
        if (bytes <= 0) {
            return -1;
        }
        
        total_received += bytes;
        
        // Check for complete line (ending with \r\n)
        if (total_received >= 2 && 
            buffer[total_received-2] == '\r' && 
            buffer[total_received-1] == '\n') {
            
            // Remove \r\n and null terminate
            buffer[total_received-2] = '\0';
            
            // Check for blank line (end of headers)
            if (strlen(buffer) == 0) {
                break;
            }
            
            // Store header line
            strncpy(headers[header_count], buffer, MAX_HEADER_SIZE - 1);
            headers[header_count][MAX_HEADER_SIZE - 1] = '\0';
            header_count++;
            
            // Reset buffer for next line
            total_received = 0;
        }
        
        // Prevent buffer overflow
        if (total_received >= BUFFER_SIZE - 1) {
            return -1;
        }
    }
    
    return header_count;
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
char* find_host_header(char headers[][MAX_HEADER_SIZE], int header_count) {
    for (int i = 0; i < header_count; i++) {
        if (strncasecmp(headers[i], "Host:", 5) == 0) {
            // Skip "Host: " and return hostname
            return headers[i] + 6;  // Skip "Host: "
        }
    }
    return NULL;
}

/**
 * @brief Handle client request
 * 
 * @param client_socket Socket connected to client
 * @return int 0 on success, -1 on error
 */
int handle_client_request(int client_socket) {
    char headers[MAX_HEADERS][MAX_HEADER_SIZE];
    char method[16], uri[256], version[16];
    
    // Read HTTP headers
    int header_count = read_http_headers(client_socket, headers);
    if (header_count < 0) {
        fprintf(stderr, "Failed to read headers\n");
        return -1;
    }
    
    if (header_count == 0) {
        fprintf(stderr, "No headers received\n");
        return -1;
    }
    
    // Parse request line (first header)
    parse_request_line(headers[0], method, uri, version);
    
    // Log request tail (last header line)
    if (header_count > 0) {
        printf("Request tail %s\n", headers[header_count - 1]);
        fflush(stdout);
    }
    
    // Find Host header
    char *hostname = find_host_header(headers, header_count);
    if (!hostname) {
        fprintf(stderr, "No Host header found\n");
        return -1;
    }
    
    // Log what we would forward
    printf("GETting %s %s\n", hostname, uri);
    fflush(stdout);
    
    return 0;
}