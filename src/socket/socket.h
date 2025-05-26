#ifndef SOCKET_H
#define SOCKET_H

/**
 * @brief Create dual-stack TCP listening socket (accepts both IPv4 and IPv6)
 * 
 * @param port Port number
 * @return int Socket file descriptor
 */
int create_listening_socket(int port);

/**
 * @brief Connect to origin server
 * 
 * @param hostname Hostname to connect to
 * @return int Socket file descriptor, or -1 on error
 */
int connect_to_server(const char *hostname);

#endif /* SOCKET_H */