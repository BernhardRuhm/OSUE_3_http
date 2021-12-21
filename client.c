#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

char *myprog;

void usage()
{
    fprintf(stderr, "usage %s: %s [-p PORT] [ -o FILE | -d DIR ] URL\n", myprog, myprog);
    exit(EXIT_FAILURE);
}


int main(int argc, char **argv)
{
    myprog = argv[0];

    int c;
    char *port = NULL;
    char *file = NULL;
    char *dir = NULL;
    char *url = NULL;
    int opt_p = 0;
    int opt_o = 0;
    int opt_d = 0;

    while ((c = getopt(argc, argv, "p:o:d")) != -1)
    {
        switch (c)
        {
        case 'p':
            port = optarg;
            opt_p++;
            break;

        case 'o':
            file = optarg;
            opt_o++;
            break;

        case 'd':
            dir = optarg;
            opt_d++;
            break;

        case '?':
            usage();
        }
    }

    if((opt_p > 1) || ((opt_o >= 1) && (opt_d >= 1)))
        usage();

    if((argc - (2 * opt_p) - (2 * opt_o) - (2 * opt_d)) != 2 )
        usage();




    /*
    struct addrinfo hints, *ai;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    int res = getaddrinfo("neverssl.com", "http", &hints, &ai);
    if (res != 0)
    {
        fprintf(stderr, "getaddrinfo failed: %s\n", gai_strerror(res));
        exit(EXIT_FAILURE);
    }
    int sockfd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
    if (sockfd != 0)
    {
        fprintf(stderr, "socket failed: %d\n", errno);
    }

    if (connect(sockfd, ai->ai_addr, ai->ai_addrlen) < 0)
    {
    }

    FILE *sockfile = fdopen(sockfd, "r+");

    freeaddrinfo(ai);
    */
    return EXIT_SUCCESS;
}