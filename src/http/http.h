#ifndef HTTP_H
#define HTTP_H

#include <stdint.h>

/* ========== Constants ========== */
// Buffer and size limits
#define BUFFER_SIZE 4096
#define MAX_HEADERS 50
#define MAX_HEADER_SIZE 1024
#define MAX_REQUEST_SIZE 2000
#define MAX_RESPONSE_SIZE (102400)

// String buffer sizes
#define MAX_METHOD_SIZE 16
#define MAX_URI_SIZE 256
#define MAX_VERSION_SIZE 16
#define MAX_HOSTNAME_SIZE 256

/**
 * @brief Read HTTP headers from socket with dynamic allocation
 * 
 * @param socket Socket to read from
 * @param headers Array to store header lines
 * @param header_count Number of headers read
 * @return int 0 on success, -1 on error
 */
int read_http_headers(int socket, char ***headers, int *header_count);

/**
 * @brief Free dynamically allocated headers
 * 
 * @param headers Array of header strings
 * @param header_count Number of headers to free
 */
void free_headers(char **headers, int header_count);

/**
 * @brief Parse HTTP request line
 * 
 * @param line Request line to parse
 * @param method Buffer to store HTTP method
 * @param uri Buffer to store URI
 * @param version Buffer to store HTTP version
 */
void parse_request_line(const char *line, char *method, char *uri, char *version);

/**
 * @brief Find Host header in headers array
 * 
 * @param headers Array of header strings
 * @param header_count Number of headers
 * @return char* Pointer to hostname, or NULL if not found
 */
char* find_host_header(char **headers, int header_count);

/**
 * @brief Parse Cache-Control header for no-cache directives
 *
 * @param value Cache-Control header value
 * @param max_age Pointer to store max-age value if found (in seconds)
 * @param has_max_age Pointer to flag indicating if max-age was found
 * @return int 1 if NOT cacheable, 0 if cacheable
 */
int parse_cache_control(const char *value, uint32_t *max_age, int *has_max_age);

/**
 * @brief Determine if response should be cached based on headers
 *
 * @param headers Full HTTP response headers
 * @param max_age Pointer to store max-age value if found
 * @param has_max_age Pointer to flag indicating if max-age was found
 * @return int 1 if cacheable, 0 if not
 */
int should_cache_response(const char *headers, uint32_t *max_age, int *has_max_age);

/**
 * @brief Build a complete request string from headers
 * 
 * @param headers Array of header strings
 * @param header_count Number of headers
 * @param request_buffer Buffer to store the complete request
 * @param request_len Pointer to store the length of the request
 */
void build_request_string(char **headers, int header_count, char *request_buffer, int *request_len);

#endif /* HTTP_H */