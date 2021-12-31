#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <stdbool.h>

static void usage()
{
    fprintf(stderr, "usage %s: server [-p PORT] [-i INDEX] DOC_ROOT\n", myprog);
    exit(EXIT_FAILURE);
}

int main(int argc, char **argv)
{
    int c;
    char *port = NULL;
    char *index = NULL;
    int opt_p = 0;
    int opt_i = 0;
    

    while((c = getopt(argc, argv, "p:i:")))
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
        default:
            usage();
            break;
        }

    }

    if((opt_p > 1) || (opt_i > 1))
        usage();
    
    

    return EXIT_SUCCESS;
}