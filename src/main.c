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

// Global variable for graceful shutdown
volatile sig_atomic_t running = 1;

void signal_handler(int sig);
int create_listening_socket(int port);

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

    // Start listening
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
    
        // TODO: HANDLE HTTP REQUEST

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

    // FOR TESTING (TO REMOVE)
    printf("Creating dual-stack listening socket (IPv4 + IPv6)...\n");

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

    // FOR TESTING (TO REMOVE)
    printf("IPv6 socket created successfully\n");

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
    
    // FOR TESTING (TO REMOVE)
    printf("Dual-stack mode enabled: socket accepts both IPv4 and IPv6\n");
    
    // Bind address to the socket
    if (bind(sockfd, res->ai_addr, res->ai_addrlen) < 0) {
        perror("bind");
        freeaddrinfo(res);
        exit(EXIT_FAILURE);
    }
    
    printf("Socket bound to port %s (listening on all interfaces)\n", service);
    
    freeaddrinfo(res);
    return sockfd;
}