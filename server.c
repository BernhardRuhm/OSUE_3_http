#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <stdbool.h>
#include <signal.h>
#include <time.h>

#define DEFAULT_PORT "8080"
#define DEFAULT_INDEX "index.html"

char *prog;
volatile __sig_atomic_t sig_recv = 0;

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
 * @brief changes value of sig_recv if SIGINT or SIGTERM ar recieved
 * @param signal recieved signal
 */
static void handle_signal(int signal)
{
    sig_recv = 1;
}

/**
 * @brief sets up the signal handler
 */
static void setup_signal_handler()
{
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_signal;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
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
        return -1;
    
    fclose(f);
    return 0;
}

static int setupt_socket(char *port)
{
    struct addrinfo hints, *ai;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    int res = getaddrinfo(NULL, port, &hints, &ai);
    if (res != 0)
    {
        fprintf(stderr, "getaddrinfo failed: %s\n", gai_strerror(res));
        freeaddrinfo(ai);
        return -1;
    }
    int sockfd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
    if (sockfd < 0)
    {
        fprintf(stderr, "socket failed: %s\n", strerror(errno));
        freeaddrinfo(ai);
        return -1;
    }

    int optval = 1;
    if(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0)
    {
        fprintf(stderr, "setsockopt failed: %s", strerror(errno));
        freeaddrinfo(ai);
        return -1;
    }

    if (bind(sockfd, ai->ai_addr, ai->ai_addrlen) < 0)
    {
        fprintf(stderr, "bind failed: %s\n", strerror(errno));
        freeaddrinfo(ai);
        return -1;
    }

    if(listen(sockfd, 1) < 0)
    {
        fprintf(stderr, "listen failed: %s\n", strerror(errno));
        return -1;
    }

    freeaddrinfo(ai);
    return sockfd;
}


/**
 * @brief Get the time object
 * 
 * @return char* 
 */
static char *get_time()
{
    char *r = malloc(200 * (sizeof(char)));
    char buf[200];
    time_t t;
    struct tm *tmp;

    t = time(NULL);
    tmp = localtime(&t);
    if(tmp == NULL)
    {
        fprintf(stderr, "localtime failed: %s\n", strerror(errno));
        free(r);
        return NULL;
    }

    if(strftime(buf, sizeof(buf),"Date: %a, %d %h %g %X %Z" , tmp) == 0)
    {
        fprintf(stderr, "strftime failed");
        free(r);
        return NULL;
    }
    strcpy(r, buf);
    return r;
}


/**
 * @brief 
 * 
 * @param s 
 * @param code 
 * @param msg 
 * @param size 
 */
static void send_response_header(FILE *s, int code, char *msg, int size, bool state)
{   
    if(state)
    {
        char *time = get_time();
        if(time == NULL)
            exit(EXIT_FAILURE);

        fprintf(s, "HTTP/1.1 %d %s\n%s \nConten-Length: %d\nConnection: close\n\r\n", code, msg, time, size);
        free(time);
    }
    else
    {
        fprintf(s, "HTTP/1.1 %d %s\n", code, msg);
    }
    fflush(s);
}


static void skip_header(FILE *s)
{
    char *line = NULL;
    size_t length = 0;
    do
    {
        getline(&line, &length, s);
    } while (strcmp(line, "\r\n") != 0);

    free(line);
}


/**
 * @brief 
 * 
 */
static FILE *verify_request(FILE *s, char *path)
{
    char *line = NULL;
    size_t length = 0;
    int whitespaces = 0;

    getline(&line, &length, s);

    //count whitespaces
    for(int i = 0; i < strlen(line); i++)
    {
        if(line[i] == ' ')
            whitespaces++;
    }
    
    if(whitespaces != 2)
    {
        send_response_header(s, 400, "Bad Request", 0, 0);
        free(line);
        return NULL;
    }

    char *copy = line;
    char *tmp = strsep(&copy, " ");

    if(strcmp(tmp, "GET") != 0)
    {   
        skip_header(s);
        send_response_header(s, 501, "Not implemented", 0, 0);
        free(line);
        return NULL;
    }

    tmp = strsep(&copy, " ");
    FILE *f;
    char *dir = malloc(strlen(path) + strlen(tmp) + 1);
    strcpy(dir, path);
    strcat(dir, tmp);
    if((f = fopen(dir, "r")) == NULL)
    {   
        skip_header(s);
        send_response_header(s, 404, "Not Found", 0, 0);
        free(line);
        free(dir);
        return NULL;
    }

    if(strcmp(copy, "HTTP/1.1\r\n") != 0)
    {
        skip_header(s);
        send_response_header(s, 400, "Bad Request", 0, 0);
        fclose(f);
        free(line);
        free(dir);
        return NULL;
    }
    
    skip_header(s);
    send_response_header(s, 200, "OK", 150, 1);

    free(line);
    free(dir);
    return f;
}

/**
 * @brief 
 * 
 * @param s 
 * @param w 
 */
static void transmit_data(FILE *s, FILE *w)
{
    char *line = NULL;
    size_t length = 0;

    while (getline(&line, &length, w) != -1)
    {
        fprintf(s, "%s", line);
    }
    fflush(s);
    free(line);
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

//setup socket
    int sockfd = setupt_socket(port);
    if(sockfd == -1)
    {
        fprintf(stderr, "setting up socket failed\n");
        exit(EXIT_FAILURE);
    }

    setup_signal_handler();

    while (!sig_recv)
    {
        int connfd = accept(sockfd, NULL, NULL);
        if(connfd < 0)
        {
            if(errno != EINTR)
            {
                fprintf(stderr, "accept failed: %s\n", strerror(errno));
                exit(EXIT_FAILURE);
            }
            //signal recieved
            continue;
        }
        
        FILE *sockfile = fdopen(connfd, "r+");
        if(sockfile == NULL)
        {
            fprintf(stderr, "opening the socket fd failed: %s", strerror(errno));
            exit(EXIT_FAILURE);
        }

        //read request and write header to client
        FILE *request = verify_request(sockfile, dir_root);
        if(request == NULL)
        {
            fclose(sockfile);
            continue;
        }
    
        //write content
        transmit_data(sockfile, request);
        

        fclose(request);
        fclose(sockfile);
    }
    
    return EXIT_SUCCESS;
}