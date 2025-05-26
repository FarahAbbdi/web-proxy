#ifndef ARGS_H
#define ARGS_H

#include <string.h>
#include <strings.h>
#include <stdint.h>

/**
 * @brief Prints usage instructions and exits the program.
 *
 * @param prog_name Name of the running program (argv[0])
 */
void print_usage(const char *prog_name);

/**
 * @brief Parses and validates command-line arguments for -p and optional -c flags.
 *
 * @param argc Argument count
 * @param argv Argument vector
 * @param port Output pointer for storing listen port
 * @param c_flag Output pointer for storing presence of -c flag (1 if set, 0 otherwise)
 */
void parse_args(int argc, char *argv[], int *port, int *c_flag);

/**
 * @brief Trims whitespace from the beginning and end of a string
 *
 * @param str String to trim
 * @return char* Pointer to the trimmed string
 */
char* trim(char *str);

/**
 * @brief Find a case-insensitive substring within a string
 *
 * @param haystack String to search in
 * @param needle String to search for
 * @return char* Pointer to the found substring, or NULL if not found
 */
char* find_case_insensitive(const char *haystack, const char *needle);

/**
 * @brief Duplicate a string (if strdup is not available)
 *
 * @param s String to duplicate
 * @return char* Newly allocated copy of the string
 */
char* my_strdup(const char *s);

#endif