#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <stdbool.h>

#define DEFAULT_PORT "8080"
#define DEFAULT_INDEX "index.html"

char *prog;

/**
 * @brief writes an usage message to stderr and the program exits
 *on EXIT_FAILURE
 * 
 */
static void usage()
{
    fprintf(stderr, "usage %s: server [-p PORT] [-i INDEX] DOC_ROOT\n", prog);
    exit(EXIT_FAILURE);
}

/**
 * @brief verifies if directory root is a valid directory
 * @param dir_root directory root
 * @return int 
 *          -1 on error
 *           0 on success      
 */
static int verify_root(char *dir_root)
{   
    FILE *f;
    if((f = fopen(dir_root, "r")) == NULL)
    {
        return -1;
    }
    fclose(f);
    return 0;
}


/**
 * @brief 
 * 
 * @param argc 
 * @param argv 
 * @return int 
 */
int main(int argc, char **argv)
{
    prog = argv[0];

//argument handling
    int c;
    char *port = NULL;
    char *index = NULL;
    int opt_p = 0;
    int opt_i = 0;
    
    while((c = getopt(argc, argv, "p:i:")) != -1)
    {
        switch (c)
        {
        case 'p':
            opt_p++;
            port = optarg;
            break;
        case 'i':
            opt_i++;
            index = optarg;
            break;
        case '?':
            usage();
        }
    }

    if((opt_p > 1) || (opt_i > 1))
        usage();

    if((argc - (2 * opt_p) - (2 *opt_i)) != 2)
        usage();
    if(port == NULL)
        port = DEFAULT_PORT;
    if(index == NULL)
        index = DEFAULT_INDEX;
    
    char *dir_root = argv[optind];
    
    if(verify_root(dir_root) == -1)
    {
        fprintf(stderr, "fopen failed: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }


    return EXIT_SUCCESS;
}