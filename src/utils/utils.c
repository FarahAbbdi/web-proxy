#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/**
 * @brief Prints usage instructions and exits the program.
 *
 * @param prog_name Name of the running program (argv[0])
 */
void print_usage(const char *prog_name)
{
    fprintf(stderr, "Usage: %s -p <listen-port> [-c]\n", prog_name);
    exit(EXIT_FAILURE);
}

/**
 * @brief Parses and validates command-line arguments for -p and optional -c flags.
 *
 * Expects at least 3 arguments: `-p <listen-port>`, and optionally `-c`.
 * If missing or invalid, prints usage and exits.
 *
 * @param argc Argument count
 * @param argv Argument vector
 * @param port Output pointer for storing listen port
 * @param c_flag Output pointer for storing presence of -c flag (1 if set, 0 otherwise)
 */
void parse_args(int argc, char *argv[], int *port, int *c_flag)
{
    *port = -1;
    *c_flag = 0;

    if (argc < 3 || argc > 4)
    {
        print_usage(argv[0]); // Invalid argument count
    }

    for (int i = 1; i < argc; i++)
    {
        if (!strcmp(argv[i], "-p") && i + 1 < argc)
        {
            // Check that the port is a number
            for (char *p = argv[i + 1]; *p; ++p)
            {
                if (!isdigit(*p))
                {
                    print_usage(argv[0]);
                }
            }
            *port = atoi(argv[++i]);
        }
        else if (!strcmp(argv[i], "-c"))
        {
            *c_flag = 1;
        }
        else
        {
            print_usage(argv[0]); // Unrecognised flag or missing value
        }
    }

    // Final validation
    if (*port <= 0)
    {
        print_usage(argv[0]);
    }
}

/**
 * @brief Trims whitespace from the beginning and end of a string
 *
 * @param str String to trim
 * @return char* Pointer to the trimmed string
 */
char* trim(char *str)
{
    if (!str) return NULL;
    
    // Trim leading space
    while(isspace((unsigned char)*str)) str++;
    
    if(*str == 0)  // All spaces?
        return str;
    
    // Trim trailing space
    char *end = str + strlen(str) - 1;
    while(end > str && isspace((unsigned char)*end)) end--;
    
    // Write new null terminator
    *(end+1) = 0;
    
    return str;
}

/**
 * @brief Find a case-insensitive substring within a string
 *
 * @param haystack String to search in
 * @param needle String to search for
 * @return char* Pointer to the found substring, or NULL if not found
 */
char* find_case_insensitive(const char *haystack, const char *needle)
{
    if (!haystack || !needle) return NULL;
    
    size_t needle_len = strlen(needle);
    size_t haystack_len = strlen(haystack);
    
    if (needle_len > haystack_len) return NULL;
    
    for (size_t i = 0; i <= haystack_len - needle_len; i++) {
        if (strncasecmp(haystack + i, needle, needle_len) == 0) {
            return (char*)(haystack + i);
        }
    }
    
    return NULL;
}

/**
 * @brief Duplicate a string (if strdup is not available)
 *
 * @param s String to duplicate
 * @return char* Newly allocated copy of the string
 */
char* my_strdup(const char *s)
{
    if (!s) return NULL;
    
    size_t len = strlen(s) + 1; // +1 for null terminator
    char *new_str = malloc(len);
    
    if (new_str) {
        memcpy(new_str, s, len);
    }
    
    return new_str;
}