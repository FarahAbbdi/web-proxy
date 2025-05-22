#include <stdio.h>
#include "utils/args/arg.h"  

/**
 * @brief Main function. 
 *
 * @param argc Argument count
 * @param argv Argument vector
 * @return int Exit status
 */
int main(int argc, char *argv[])
{
    int port;
    int c_flag;

    // Parse command-line arguments
    parse_args(argc, argv, &port, &c_flag);

    // For demonstration: print parsed values
    printf("Listen port: %d\n", port);
    printf("-c flag: %s\n", c_flag ? "set" : "not set");

    // TODO: Add proxy logic here

    return 0;
}