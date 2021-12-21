#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#define DEFAULT_PORT "http"
#define DEFAULT_FILE "index.html"

char *myprog;

static void usage()
{
    fprintf(stderr, "usage %s: %s [-p PORT] [ -o FILE | -d DIR ] URL\n", myprog, myprog);
    exit(EXIT_FAILURE);
}

static void error_exit(char *msg)
{
    fprintf(stderr, "%s: %s", myprog, msg);
    exit(EXIT_FAILURE);
}

static int setup_socket(char *host, char *port)
{
    struct addrinfo hints, *ai;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    int res = getaddrinfo(host, port, &hints, &ai);
    if (res != 0)
    {
        fprintf(stderr, "getaddrinfo failed: %s\n", gai_strerror(res));
        return -1;
    }
    int sockfd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
    if (sockfd < 0)
    {
        fprintf(stderr, "socket failed: %d\n", errno);
        return -1;
    }

    if (connect(sockfd, ai->ai_addr, ai->ai_addrlen) < 0)
    {
    }
    freeaddrinfo(ai);
    return sockfd;
}


static void send_request(char *path, char *host, FILE *s)
{
    fprintf(s,"GET /%s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n", path, host);
    fflush(s);
}

static void validate_header(FILE *s)
{
    char *line = NULL;
    size_t len = 0;

    if(getline(&line, &len, s) == -1)
        error_exit("read from socket failed");

    char *tmp1 = strsep(&line, " ");
    char *tmp2 = strsep(&line, " ");

}

static void read_and_write(FILE *s, FILE *f)
{
    char *line = NULL;
    size_t len = 0;

    while(getline(&line, &len, s) != -1){
        fprintf(f, "%s", line);
    }

    free(line);
}

int main(int argc, char **argv)
{
    myprog = argv[0];

    int c;
    char *port = NULL;
    char *file = NULL;
    char *dir = NULL;
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

    char *url = argv[optind];

    if(strncmp(url, "http://", 7) != 0)
    {
        error_exit("url must contain http://\n");
    }
    char *url_wihtout_http = url + 7;
    char *host = strsep(&url_wihtout_http, ";/?:@=&");
    char *path = url_wihtout_http;
   
    int sockfd = setup_socket(host, DEFAULT_PORT);
    if(sockfd == -1)
    {
        error_exit("creating socket failed\n");
    }

    FILE *sockfile = fdopen(sockfd, "r+");
    send_request(path, host, sockfile);

    validate_header(sockfile);
    //read_and_write(sockfile, stdout);
    fclose(sockfile);
    
    return EXIT_SUCCESS;
}