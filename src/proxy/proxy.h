#ifndef PROXY_H
#define PROXY_H

// Global flag for caching
extern int g_cache_enabled;

/**
 * @brief Handle client request 
 * 
 * @param client_socket Socket connected to client
 * @return int 0 on success, -1 on error
 */
int handle_client_request(int client_socket);

/**
 * @brief Forward response from server to client
 * 
 * @param server_socket Socket connected to origin server
 * @param client_socket Socket connected to client
 * @param request Complete request string (for caching)
 * @param request_len Length of request
 * @param hostname Hostname of the server
 * @param uri URI being requested
 * @param stale Whether this is replacing a stale cache entry
 * @return int 0 on success, -1 on error
 */
int forward_response(int server_socket, int client_socket, const char *request, 
                    int request_len, const char *hostname, const char *uri, int stale);

#endif /* PROXY_H */