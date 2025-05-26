#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <sys/socket.h>

#include "http.h"
#include "utils.h"

/**
 * @brief Read HTTP headers from socket with dynamic allocation
 * 
 * @param socket Socket to read from
 * @param headers Array to store header lines
 * @param header_count Number of headers read
 * @return int 0 on success, -1 on error
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
 * @brief Parse Cache-Control header for no-cache directives
 *
 * @param value Cache-Control header value
 * @param max_age Pointer to store max-age value if found (in seconds)
 * @param has_max_age Pointer to flag indicating if max-age was found
 * @return int 1 if NOT cacheable, 0 if cacheable
 */
int parse_cache_control(const char *value, uint32_t *max_age, int *has_max_age) {
    if (!value) return 0;

    char *copy = my_strdup(value);
    if (!copy) return 0;

    for (char *p = copy; *p; p++) *p = tolower((unsigned char)*p);

    char *token = strtok(copy, ",");
    while (token) {
        char *directive = trim(token);
        if (strcmp(directive, "private") == 0 ||
            strcmp(directive, "no-store") == 0 ||
            strcmp(directive, "no-cache") == 0 ||
            strcmp(directive, "must-revalidate") == 0 ||
            strcmp(directive, "proxy-revalidate") == 0 ||
            strncmp(directive, "max-age=0", 9) == 0) {
            free(copy);
            return 1;
        }
        
        // Handle max-age directive
        if (strncmp(directive, "max-age=", 8) == 0) {
            char *age_str = directive + 8;
            uint32_t age_val = (uint32_t)strtoul(age_str, NULL, 10);
            *max_age = age_val;
            *has_max_age = 1;
        }
        
        token = strtok(NULL, ",");
    }

    free(copy);
    return 0;
}

/**
 * @brief Determine if response should be cached based on headers
 *
 * @param headers Full HTTP response headers
 * @param max_age Pointer to store max-age value if found
 * @param has_max_age Pointer to flag indicating if max-age was found
 * @return int 1 if cacheable, 0 if not
 */
int should_cache_response(const char *headers, uint32_t *max_age, int *has_max_age) {
    char *cc = find_case_insensitive(headers, "cache-control:");
    if (!cc) return 1;

    cc += strlen("cache-control:");
    while (*cc && isspace((unsigned char)*cc)) cc++;

    char *end = strstr(cc, "\r\n");
    if (!end) return 1;

    size_t len = end - cc;
    char *value = malloc(len + 1);
    if (!value) return 1;

    strncpy(value, cc, len);
    value[len] = '\0';

    int result = !parse_cache_control(value, max_age, has_max_age);
    free(value);
    return result;
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