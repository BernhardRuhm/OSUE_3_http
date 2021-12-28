/**
*
*@file client.c
*@author Bernhard Ruhm
*@date last edit 12-28-2021
*@brief client program to communicate with http server
*@details This program implements a client that communicates with a http server.
*The client takes an URL as input and connects to the corresponding host. After the connection
*was established, the clients sends a request for a file, specified in the URL by using the http method GET.
*The requested content is either written to a specified file or to stdout
*
*SYNOPSIS
*client [-p PORT] [ -o FILE | -d DIR ] URL
*
*-p: specify port to connect with server, on default http
*-o: specify file to write requested contet
*-d: specify a directory to write requested content, file name equals name of requested file
*    if a directory was requested, the file name is "index.html"
*if neither -d or -o are given as input, transmitted data ist written to stdout
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

#define DEFAULT_PORT "http"
#define DEFAULT_FILE "index.html"

char *myprog;

/**
*usage function
*@brief writes an usage message to stderr and the program exits
*on EXIT_FAILURE
*
**/
static void usage()
{
    fprintf(stderr, "usage %s: %s [-p PORT] [ -o FILE | -d DIR ] URL\n", myprog, myprog);
    exit(EXIT_FAILURE);
}

/**
*print error message and exit programm
*@brief writes an error message to stderr and exits the program
*@details this function writes a given error message to stderr
*@param msg message printed to stderr
*
**/
static void error_exit(char *msg)
{
    fprintf(stderr, "%s: %s", myprog, msg);
    exit(EXIT_FAILURE);
}
/**
 * @brief extracts hostname and file from url
 * @details this function extracts the hostname and requested file or directory
 * from the url
 * @param url url given as input
 * @param host hostname
 * @param request requested file or directory
 */
static void extract_host_request(char *url, char *host, char *request)
{
    char *url_wihtout_http = url + 7;
    char *tmp = strsep(&url_wihtout_http, ";/?:@=&");

    strcpy(host, tmp);

    if(url_wihtout_http == NULL)
        strcpy(request, "");
    else
        strcpy(request, url_wihtout_http);
    
}
/**
 * @brief checks last char of a string
 * @details function verifies, if last char of a string equals a specfified char
 * or if the string is empty
 * @param s string
 * @param c char
 * @return true if last char equals c or string ist empty
 * @return false otherwise
 */
static bool ends_with(char *s, char c)
{
    int size = strlen(s);
    if(size == 0)
        return true;
    
    if(s[size - 1] == '/')
        return true;
    
    return false;
}

/**
 * @brief creates output file pointer
 * @details creates the outfile pinter depending on the program input
 * @param request requested file or directory in url
 * @param file file from option -o
 * @param dir directory from option -d
 * @return FILE* created file pointer or stdout
 */
static FILE *output_file(char *request, char *file, char *dir)
{
    if((file == NULL) && (dir == NULL))
        return stdout;

    FILE *f;

    if(file != NULL)
    {
        if((f = fopen(file, "w")) == NULL)
        {
            fprintf(stderr, "[%s] ERROR: fopen failed : %s\n", file, strerror(errno));
            exit(EXIT_FAILURE);
        }
        return f;
    }

    if(ends_with(request, '/'))
    {   
        strcat(dir, "index.html");
        if((f = fopen(dir, "w")) == NULL)
        {
            fprintf(stderr, "[%s] ERROR: fopen failed : %s\n", dir, strerror(errno));
            exit(EXIT_FAILURE);
        }
    }
    else
    {
        char *tmp =  strrchr(request, '/');

        if(tmp != NULL)
            strcat(dir, tmp);
        else
            strcat(dir, request);
        if((f = fopen(dir, "w")) == NULL)
        {
            fprintf(stderr, "[%s] ERROR: fopen failes : %s\n", dir, strerror(errno));
            exit(EXIT_FAILURE);
        }
    }
    return f;

}

/**
 * @brief Set the up socket object
 * @param host host from url
 * @param port port from option -p or default port http
 * @return int socket file descriptor
 */
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
        fprintf(stderr, "socket failed: %s\n", strerror(errno));
        return -1;
    }

    if (connect(sockfd, ai->ai_addr, ai->ai_addrlen) < 0)
    {
        fprintf(stderr, "connect failed: %s\n", strerror(errno));
        return -1;
    }
    freeaddrinfo(ai);
    return sockfd;
}

/**
 * @brief sends request to http server
 * @param request requested file or directory 
 * @param host host from url
 * @param s file pointer of socket
 */
static void send_request(char *request, char *host, FILE *s)
{
    fprintf(s,"GET /%s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n", request, host);
    fflush(s);
}

/**
 * @brief verifies the transmitted http header
 * @details this function verifies the tranmitted http header.
 * If the header doesnt equal HTTP/1.1 200 'status message' the program 
 * exits on error
 * @param s file pointer of socket
 * @return int 0 on success, 2 and 3 on error
 */
static int validate_header(FILE *s)
{
    char *line = NULL;
    char *end_ptr;
    size_t len = 0;

    getline(&line, &len, s);
    char *copy = line;
    char *tmp = strsep(&copy, " ");
    
    if((strcmp(tmp, "HTTP/1.1") != 0) || (errno == EINVAL))
    {
        fprintf(stderr, "Protocoll error\n");
        free(line);
        return 2;
    }
    
    tmp = strsep(&copy, " ");
    int status = strtol(tmp, &end_ptr, 10);
    if(status != 200)
    {
        fprintf(stderr, "response status not 200, recieved %d %s\n", status, copy);
        free(line);
        return 3;
    }

    //skip header
    while (strcmp(line, "\r\n") != 0)
    {
        getline(&line, &len, s);
    }

    free(line);
    return 0;
}

/**
 * @brief reads transmitted data from socket und writes content to a file
 * @param s file pointer of socket
 * @param f file pointer to write data
 */
static void read_and_write(FILE *s, FILE *f)
{
    char *line = NULL;
    size_t len = 0;

    while(getline(&line, &len, s) != -1){
        fprintf(f, "%s", line);
    }

    free(line);
}

/**
 * @brief client http connection
 * @details The main function provides the main 
 *functionality of this program, by calling the functions extract_host_request(),
 *output_file(), setup_socket(), send_request(), validate_header() and read_and_write()
 * global variables: prog
 * @param argc argument count
 * @param argv argument counter
 * @return int EXIT_SUCCESS on successfull termination
 *             EXIT_FAILURE on failed termination
 */
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
    

    while ((c = getopt(argc, argv, "p:o:d:")) != -1)
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

    char *host = malloc(strlen(url - 7));
    char *request = malloc(strlen(url - 7));
    extract_host_request(url, host, request);
    
    if(port == NULL)
        port = DEFAULT_PORT;

    int sockfd = setup_socket(host, port);

    if(sockfd == -1)
        error_exit("creating socket failed\n");
    
    FILE *w_file = output_file(request, file, dir);
    FILE *sockfile = fdopen(sockfd, "r+");

    send_request(request, host, sockfile);

    int h = validate_header(sockfile);
    if(h != 0)
        exit(h);

    read_and_write(sockfile, w_file);

    fclose(sockfile);
    fclose(w_file);
    free(host);
    free(request);
    
    return EXIT_SUCCESS;
}