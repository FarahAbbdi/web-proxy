#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <sys/socket.h>
#include <time.h>

#include "proxy.h"
#include "http.h"
#include "cache.h"
#include "socket.h"

// Using global cache flag from main.c

/**
 * @brief Handle client request 
 * 
 * @param client_socket Socket connected to client
 * @return int 0 on success, -1 on error
 */
int handle_client_request(int client_socket) {
    char **headers = NULL;
    int header_count = 0;
    char method[MAX_METHOD_SIZE], uri[MAX_URI_SIZE], version[MAX_VERSION_SIZE];
    char *hostname = NULL;
    
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

    hostname = find_host_header(headers, header_count);
    if (!hostname) {
        fprintf(stderr, "No Host header found\n");
        free_headers(headers, header_count);
        return -1;
    }

    // Do not cache non-GET methods
    if (strcasecmp(method, "GET") != 0) {
        goto proxy_request;
    }

    // Log request tail (last header line)
    if (header_count > 0) {
        printf("Request tail %s\n", headers[header_count - 1]);
        fflush(stdout);
    }
    
    // Build complete request string for cache lookup
    char request_buffer[MAX_REQUEST_SIZE];
    int request_len = 0;
    build_request_string(headers, header_count, request_buffer, &request_len);

    int stale = 0;
    // Check if request is cacheable (less than 2000 bytes)
    if (g_cache_enabled && request_len < MAX_REQUEST_SIZE) {
        // Look for request in cache
        cache_entry *entry = find_in_cache(request_buffer, request_len);
        
        if (entry) {
            int is_stale = 0;
            
            // Only check expiration if max-age was specified
            if (entry->has_max_age) {
                time_t age = time(NULL) - entry->cached_time;
                if (age > entry->max_age) {
                    is_stale = 1;
                }
            }
            // If no max-age, entry is always fresh
            
            if (!is_stale) {
                // Serve from cache
                printf("Serving %s %s from cache\n", entry->host, entry->uri);
                fflush(stdout);
                move_to_front(entry);
                
                if (send(client_socket, entry->response, entry->response_size, 0) < 0) {
                    free_headers(headers, header_count);
                    return -1;
                }
                
                free_headers(headers, header_count);
                return 0;
            } else {
                // Entry is stale
                printf("Stale entry for %s %s\n", entry->host, entry->uri);
                fflush(stdout);
                stale = 1;
                // Continue to fetch fresh copy
            }
        }
        else{
            if (cache.count == CACHE_SIZE) {
                entry = evict_lru();
            }
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
        int result = forward_response(server_socket, client_socket, request_buffer, request_len, hostname, uri, stale);
        
        close(server_socket);
        free_headers(headers, header_count);
        return result;
}

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
                    int request_len, const char *hostname, const char *uri, int stale) {
    char buffer[BUFFER_SIZE * 2];
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
    
    // Check if we should cache this response
    int should_cache = 0;
    int basic_cacheable = (g_cache_enabled && request_len < MAX_REQUEST_SIZE && 
                          content_length >= 0 && 
                          content_length <= MAX_RESPONSE_SIZE);
    uint32_t max_age = 0; 
    int has_max_age = 0;
    if (basic_cacheable) {
        should_cache = should_cache_response(header_buffer, &max_age, &has_max_age);
        if (!should_cache) {
            // Log that we're not caching due to Cache-Control
            printf("Not caching %s %s\n", hostname, uri);
            fflush(stdout);
        }
    }

    // Prepare for possible caching
    char *response_buffer = NULL;
    int response_size = total_received;
    
    if (should_cache) {
        // Allocate buffer for the complete response
        response_buffer = malloc(MAX_RESPONSE_SIZE);
        if (response_buffer) {
            // Copy headers to response buffer
            memcpy(response_buffer, header_buffer, total_received);
        } else {
            should_cache = 0; 
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
            if (should_cache && response_buffer && response_size + bytes <= MAX_RESPONSE_SIZE) {
                memcpy(response_buffer + response_size, buffer, bytes);
                response_size += bytes;
            } else if (should_cache) {
                should_cache = 0; // Response too large to cache
            }
            
            remaining -= bytes;
        }
    } else if (content_length < 0) {
        // No Content-Length header, read until connection closes
        // We won't cache these responses
        should_cache = 0;
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
    
    if (stale) {
        if (!should_cache) {
            evict_entry(request, 1);
        }
        else {
            evict_entry(request, 0);
        }
    }
     
    // Add to cache if we should cache (Stage 3: only if Cache-Control allows it)
    if (should_cache && response_buffer && request_len < MAX_REQUEST_SIZE && response_size <= MAX_RESPONSE_SIZE) {
        add_to_cache(request, request_len, response_buffer, response_size, hostname, uri, max_age, has_max_age);
    }

    if (response_buffer) {
        free(response_buffer);
    }
    
    return 0;
}