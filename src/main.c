#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200112L

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>

#include "utils/utils.h"
#include "socket/socket.h"
#include "proxy/proxy.h"
#include "cache/cache.h"

/* Constants */
#define BACKLOG 10

/* Global variables */
int g_cache_enabled = 0;

/**
 * @brief Main function. 
 *
 * @param argc Argument count
 * @param argv Argument vector
 * @return int Exit status
 */
int main(int argc, char *argv[]) {
    int port;
    
    parse_args(argc, argv, &port, &g_cache_enabled);
    
    if (g_cache_enabled) {
        init_cache();
    }
    
    int listen_socket = create_listening_socket(port);
    if (listen_socket < 0) {
        fprintf(stderr, "Failed to create listening socket\n");
        return EXIT_FAILURE;
    }
    
    if (listen(listen_socket, BACKLOG) < 0) {
        perror("listen failed");
        close(listen_socket);
        return EXIT_FAILURE;
    }
    
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
        
        if (handle_client_request(client_socket) < 0) {
            fprintf(stderr, "Failed to handle client request\n");
        }
        
        close(client_socket);
    }
    
    close(listen_socket);
    printf("shutdown complete");
    return 0;
}