#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "cache.h"

// Global cache instance
lru_cache cache;

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
 * @return cache_entry* Pointer to the evicted entry 
 */
cache_entry* evict_lru() {
    if (!cache.tail) {
        // Cache is empty
        return &cache.entries[0];
    }
    
    cache_entry *to_evict = cache.tail;
    
    // Log eviction
    printf("Evicting %s %s from cache\n", to_evict->host, to_evict->uri);
    fflush(stdout);
    
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
 * @param max_age Max-age value from Cache-Control header
 * @param has_max_age Whether max-age was specified
 */
void add_to_cache(const char *request, int request_len, const char *response, int response_len, 
                  const char *host, const char *uri, uint32_t max_age, int has_max_age) {
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
    
    entry->max_age = max_age;
    entry->cached_time = time(NULL);
    entry->has_max_age = has_max_age;
    
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

/**
 * @brief Evict a specific entry from the cache
 * 
 * @param request Request string to evict
 * @param should_print Whether to print eviction message (According to Task 4 Logical Placement)
 */
void evict_entry(const char *request, int should_print) {
    cache_entry *entry = find_in_cache(request, strlen(request));
    
    // Check if entry exists
    if (!entry) {
        fprintf(stderr, "Warning: Attempted to evict non-existent entry\n");
        fflush(stderr);
        return;
    }
    
    // Log eviction
    if (should_print) {
        printf("Evicting %s %s from cache\n", entry->host, entry->uri);
        fflush(stdout);
    }
    
    // Remove from LRU linked list
    if (entry->prev) {
        entry->prev->next = entry->next;
    } else {
        // This was the head
        cache.head = entry->next;
    }
    
    if (entry->next) {
        entry->next->prev = entry->prev;
    } else {
        // This was the tail
        cache.tail = entry->prev;
    }
    
    // Mark as invalid and clear pointers
    entry->valid = 0;
    entry->prev = NULL;
    entry->next = NULL;
    
    cache.count--;
}