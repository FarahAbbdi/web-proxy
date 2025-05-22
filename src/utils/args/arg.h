#ifndef ARGS_H
#define ARGS_H

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

#endif