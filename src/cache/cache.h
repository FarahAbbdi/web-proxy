#ifndef CACHE_H
#define CACHE_H

#include <stdint.h>
#include <time.h>

#define CACHE_SIZE 10

#ifndef MAX_REQUEST_SIZE
#define MAX_REQUEST_SIZE 2000
#endif

#ifndef MAX_RESPONSE_SIZE
#define MAX_RESPONSE_SIZE (102400)
#endif

#ifndef MAX_HOSTNAME_SIZE
#define MAX_HOSTNAME_SIZE 256
#endif

#ifndef MAX_URI_SIZE
#define MAX_URI_SIZE 256
#endif

/**
 * Cache entry structure for storing HTTP requests and responses
 */
typedef struct cache_entry {
    // Request and response data
    char request[MAX_REQUEST_SIZE];      
    char response[MAX_RESPONSE_SIZE];    
    int response_size;
    
    // Request metadata
    char host[MAX_HOSTNAME_SIZE];          
    char uri[MAX_URI_SIZE];           
    
    // Cache control
    int valid;
    uint32_t max_age;
    time_t cached_time;
    int has_max_age;
    
    // LRU linked list pointers
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
extern lru_cache cache;

/**
 * @brief Initialise the LRU cache
 */
void init_cache();

/**
 * @brief Add a new entry to the cache
 * 
 * @param request Request string
 * @param request_len Length of the request
 * @param response Response data
 * @param response_len Length of the response
 * @param host Hostname from the request
 * @param uri URI from the request
 * @param max_age Max-age value from Cache-Control header
 * @param has_max_age Whether max-age was specified
 */
void add_to_cache(const char *request, int request_len, const char *response, int response_len, 
                  const char *host, const char *uri, uint32_t max_age, int has_max_age);

/**
 * @brief Find a request in the cache
 * 
 * @param request Request string to look for
 * @param request_len Length of the request
 * @return cache_entry* Pointer to cache entry if found, NULL otherwise
 */
cache_entry* find_in_cache(const char *request, int request_len);

/**
 * @brief Move a cache entry to the front of the LRU list (most recently used)
 * 
 * @param entry Cache entry to move
 */
void move_to_front(cache_entry *entry);

/**
 * @brief Evict a specific entry from the cache
 * 
 * @param request Request string to evict
 * @param should_print Whether to print eviction message
 */
void evict_entry(const char *request, int should_print);

/** Evict the least recently used cache entry
* 
* @return cache_entry* Pointer to the evicted entry (now invalid)
*/
cache_entry* evict_lru();

#endif /* CACHE_H */