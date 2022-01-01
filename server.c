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
        return -1;
    }
    int sockfd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
    if (sockfd < 0)
    {
        fprintf(stderr, "socket failed: %s\n", strerror(errno));
        return -1;
    }

    if (bind(sockfd, ai->ai_addr, ai->ai_addrlen) < 0)
    {
        fprintf(stderr, "bind failed: %s\n", strerror(errno));
        return -1;
    }
    freeaddrinfo(ai);
    return sockfd;
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

    setup_signal_handler();

    while (!sig_recv)
    {
        
    }
    
    
    return EXIT_SUCCESS;
}