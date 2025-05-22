#include "arg.h"
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