/**
*
*@file server.c
*@author Bernhard Ruhm
*@date last edit 01-06-2021
*@brief server program to simulate http server
*@details This program sets up a http server. It takes a directory as input, which is
*set to be the root directory of the server. After the socket has been set up, it waits 
*for clients to connect. After a successfull connection, the server sends a corresponding 
*response header plus the requested file. If the connection fails, the server sends a error
*code.
*
*SYNOPSIS
*server [-p PORT] [-i INDEX] DOC_ROOT
*
**/
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

#define ERROR(m, e) \
    {               \
        perror(m);  \
        exit(e);    \
    }

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
 * @brief parsing arguments
 * @details
 * -p: port to set up connection
 * -i: filename to transmit, if directory was requested
 * @param argc argument counter
 * @param argv argument vector
 * @param port specified port
 * @param index specified index
 */
static void argument_handling(int argc, char **argv, char **port, char **index)
{
    int c;
    int opt_p = 0;
    int opt_i = 0;

    while ((c = getopt(argc, argv, "p:i:")) != -1)
    {
        switch (c)
        {
        case 'p':
            opt_p++;
            *port = optarg;
            break;
        case 'i':
            opt_i++;
            *index = optarg;
            break;
        case '?':
            usage();
        }
    }

    if ((opt_p > 1) || (opt_i > 1))
        usage();

    if ((argc - (2 * opt_p) - (2 * opt_i)) != 2)
        usage();

    if (*port == NULL)
        *port = DEFAULT_PORT;
    if (*index == NULL)
        *index = DEFAULT_INDEX;
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
    if ((f = fopen(dir_root, "r")) == NULL)
        return -1;

    fclose(f);
    return 0;
}
/**
 * @brief Set the up socket 
 * @param port port from option -p or default port http
 * @return int socket file descriptor
 */

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
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0)
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

    if (listen(sockfd, 1) < 0)
    {
        fprintf(stderr, "listen failed: %s\n", strerror(errno));
        freeaddrinfo(ai);
        return -1;
    }

    freeaddrinfo(ai);
    return sockfd;
}

/**
 * @brief checks if string equals file or directory
 * @param s string to verify
 * @return true 
 * @return false 
 */
static bool is_file(char *s)
{
    int size = strlen(s);
    if (size == 0)
        return false;

    if (s[size - 1] == '/')
        return false;

    return true;
}

/**
 * @brief get the number of bytes from a file
 * @param f file
 * @return long -1 on error, else size
 */
static long get_file_size(FILE *f)
{
    if(fseek(f, 0, SEEK_END) == -1)
        return -1;
    long size = ftell(f);
    if(fseek(f, 0, SEEK_SET) == -1)
        return -1;
    return size;
}

/**
 * @brief Get the time
 * 
 * @return char* time formatted string
 *         NULL on error
 */
static char *get_time()
{
    char buf[50];
    time_t t;
    struct tm *tmp;

    t = time(NULL);
    tmp = localtime(&t);
    if (tmp == NULL)
    {
        fprintf(stderr, "localtime failed: %s\n", strerror(errno));
        return NULL;
    }

    if (strftime(buf, sizeof(buf), "Date: %a, %d %h %g %X %Z", tmp) == 0)
    {
        fprintf(stderr, "strftime failed");
        return NULL;
    }
    char *r = malloc(strlen(buf) + 1);
    strcpy(r, buf);
    return r;
}

/**
 * @brief sends header to client
 * @details on successfull request, send Header:
 *  HTTP/1.1 200 OK
 *  Date: date
 *  Content-Length: number of bytes
 *  Connection: close
 * @param s socket file pointer
 * @param code response code
 * @param msg response message
 * @param size file size
 */
static void send_response_header(FILE *s, int code, char *msg, long size)
{
    char *time = get_time();
    if (time == NULL)
        exit(EXIT_FAILURE);

    fprintf(s, "HTTP/1.1 %d %s\r\n%s \r\nContent-Length: %ld\r\nConnection: close\r\n\r\n", code, msg, time, size);
    free(time);
    fflush(s);
}

/**
 * @brief send header to client
 * @details on a failed request, send error code and message to client
 * @param s socket file pointer
 * @param code error code
 * @param msg error message
 */
static void send_error_code(FILE *s, int code, char *msg)
{
    fprintf(s, "HTTP/1.1 %d %s\r\n", code, msg);
    fflush(s);
}

/**
 * @brief skip remaining content of request
 * 
 * @param s socket file pointer
 */
static void skip_request(FILE *s)
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
 * @brief verifies request
 * @details verifies a clients request. Request must contain GET,
 * the requested file or dir and HTTP/1.1. In either successfull or failed request,
 * the server sends a corresponding response header.
 * @param s socket file pointer
 * @param path path dor root dir
 * @param index name of index file
 * @return FILE* file pointer of requested file
 *         NULL on error
 */
static FILE *verify_request(FILE *s, char *path, char *index)
{
    char *line = NULL;
    size_t length = 0;
    int whitespaces = 0;

    getline(&line, &length, s);

    //count whitespaces
    for (int i = 0; i < strlen(line); i++)
    {
        if (line[i] == ' ')
            whitespaces++;
    }

    if (whitespaces != 2)
    {
        send_error_code(s, 400, "Bad Request");
        free(line);
        return NULL;
    }

    char *copy = line;
    char *tmp = strsep(&copy, " ");

    if (strcmp(tmp, "GET") != 0)
    {
        skip_request(s);
        send_error_code(s, 501, "Not implemented");
        free(line);
        return NULL;
    }

    tmp = strsep(&copy, " ");
    FILE *f;
    char *dir = malloc(strlen(path) + strlen(tmp) + 1);
    strcpy(dir, path);
    strcat(dir, tmp);

    if (!is_file(dir))
    {
        dir = realloc(dir, strlen(dir) + strlen(index) + 1);
        strcat(dir, index);
    }
    if ((f = fopen(dir, "r")) == NULL)
    {
        skip_request(s);
        send_error_code(s, 404, "Not Found");
        fflush(stdout);
        free(line);
        free(dir);
        return NULL;
    }

    if (strcmp(copy, "HTTP/1.1\r\n") != 0)
    {
        skip_request(s);
        send_error_code(s, 400, "Bad Request");
        fclose(f);
        free(line);
        free(dir);
        return NULL;
    }

    skip_request(s);
    long size =  get_file_size(f);
    if(size == -1)
        ERROR("get_file_size failed", EXIT_FAILURE);
    send_response_header(s, 200, "OK", size);

    free(line);
    free(dir);
    return f;
}

/**
 * @brief transmit content of requested file
 * 
 * @param s socket file pointer
 * @param w request file pointer
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
 * @brief server http connection
 * @details The main function provides the main functionality of this program,
 * by calling the functions argument_handling, verify_request, transmit_data and
 * setup_signal_handler
 * @param argc argument count
 * @param argv argument vector
 * @return int EXIT_SUCCESS on successfull termination
 *             EXIT_FAILURE on failed termination
 */
int main(int argc, char **argv)
{
    prog = argv[0];

    char *port = NULL;
    char *index = NULL;
    argument_handling(argc, argv, &port, &index);
    
    char *dir_root = argv[optind];

    if (verify_root(dir_root) == -1)
        ERROR("opening root dir failed", EXIT_FAILURE);

    //setup socket
    int sockfd = setupt_socket(port);
    if (sockfd == -1)
    {
        fprintf(stderr, "setting up socket failed\n");
        exit(EXIT_FAILURE);
    }

    setup_signal_handler();

    while (!sig_recv)
    {
        int connfd = accept(sockfd, NULL, NULL);
        if (connfd < 0)
        {   //other error
            if (errno != EINTR)
                ERROR("accept failed", EXIT_FAILURE);

            //signal recieved
            continue;
        }

        FILE *sockfile = fdopen(connfd, "r+");
        if (sockfile == NULL)
            ERROR("opening the socket fd failed", EXIT_FAILURE);

        FILE *request = verify_request(sockfile, dir_root, index);
        if (request == NULL)
        {
            fclose(sockfile);
            continue;
        }
        transmit_data(sockfile, request);

        fclose(request);
        fclose(sockfile);
    }

    return EXIT_SUCCESS;
}
