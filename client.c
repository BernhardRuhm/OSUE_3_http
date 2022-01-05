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

#define ERROR(m, e) \
    {               \
        perror(m);  \
        exit(e);    \
    }

char *prog;



/**
*usage function
*@brief writes an usage message to stderr and the program exits
*on EXIT_FAILURE
*
**/
static void usage()
{
    fprintf(stderr, "usage %s: client [-p PORT] [-o FILE | -d DIR] URL\n", prog);
    exit(EXIT_FAILURE);
}

/**
 * @brief parsing arguments
 *-p: specify port to connect with server, on default http
 *-o: specify file to write requested contet
 *-d: specify a directory to write requested content, file name equals name of requested file
 *    if a directory was requested, the file name is "index.html"
 *if neither -d or -o are given as input, transmitted data ist written to stdout
 * 
 * @param argc argument counter
 * @param argv argument vector
 * @param port specified port
 * @param file specified filename
 * @param dir specified directory
 */

static void argument_handling(int argc, char **argv, char **port, char **file, char **dir)
{
    int c;
    int opt_p = 0;
    int opt_o = 0;
    int opt_d = 0;

    while ((c = getopt(argc, argv, "p:o:d:")) != -1)
    {
        switch (c)
        {
        case 'p':
            *port = optarg;
            opt_p++;
            break;

        case 'o':
            *file = optarg;
            opt_o++;
            break;

        case 'd':
            *dir = optarg;
            opt_d++;
            break;

        case '?':
            usage();
        }
    }

    if ((opt_p > 1) || ((opt_o >= 1) && (opt_d >= 1)))
        usage();

    if ((argc - (2 * opt_p) - (2 * opt_o) - (2 * opt_d)) != 2)
        usage();

     if (*port == NULL)
        *port = DEFAULT_PORT;

}


/**
 * @brief extract hostfrom the url
 * @param url url given as input
 * @param host hostname
 * @param request requested file or directory
 */
static void extract_host_request(char *url, char *host, char *request)
{
    char *url_wihtout_http = url + 7;
    char *tmp = strsep(&url_wihtout_http, ";/?:@=&");

    strcpy(host, tmp);
    //url_without_http is NULL if host doesnt end with '/'
    //e.g: http://nonhttps.com
    if (url_wihtout_http == NULL)
        strcpy(request, "");
    else
        strcpy(request, url_wihtout_http);
}
/**
 * @brief checks if string equals file or directory
 * 
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
 * @brief creates output file pointer
 * @details creates the outfile pinter depending on the program input
 * @param request requested file or directory in url
 * @param file file from option -o
 * @param dir directory from option -d
 * @return FILE* created file pointer, stdout or NULL on error
 */
static FILE *output_file(char *request, char *file, char *dir)
{
    if ((file == NULL) && (dir == NULL))
        return stdout;

    FILE *f;

    if (file != NULL)
    { //option -o was given
        f = fopen(file, "w");
    }
    else
    {//option -d was given
        if(is_file(dir))
        {   //dir doesnt end with '/'
            strcat(dir, "/");
        }
        if (!is_file(request))
        {
            strcat(dir, "index.html");
            f = fopen(dir, "w");
        }
        else
        {
            char *tmp = strrchr(request, '/');
            //tmp is NULL if file follows right after host
            //e.g.: http://nonhttps.com/index.html
            if (tmp != NULL)
                strcat(dir, tmp);
            else
                strcat(dir, request);

            f = fopen(dir, "w");
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
        freeaddrinfo(ai);
        return -1;
    }

    if (connect(sockfd, ai->ai_addr, ai->ai_addrlen) < 0)
    {
        fprintf(stderr, "connect failed: %s\n", strerror(errno));
        freeaddrinfo(ai);
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
    fprintf(s, "GET /%s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n", request, host);
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
    char *tmp2 = strsep(&copy, " ");
    int status = strtol(tmp2, &end_ptr, 10);

    if ((strcmp(tmp, "HTTP/1.1") != 0) || (errno == EINVAL))
    {
        fprintf(stderr, "Protocoll error\n");
        free(line);
        return 2;
    }

    if (status != 200)
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
static void transmit_content(FILE *s, FILE *f)
{
    char *line = NULL;
    size_t len = 0;

    while (getline(&line, &len, s) != -1)
    {
        fprintf(f, "%s", line);
    }
    fflush(f);
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
    prog = argv[0];

    char *port = NULL;
    char *file = NULL;
    char *dir = NULL;

    argument_handling(argc, argv, &port, &file, &dir);

    char *url = argv[optind];

    if (strncmp(url, "http://", 7) != 0)
    {
        fprintf(stderr, "url must contain http://\n");
        exit(EXIT_FAILURE);
    }

    char *host = malloc(strlen(url));
    char *request = malloc(strlen(url));
    extract_host_request(url, host, request);

    int sockfd = setup_socket(host, port);
    if (sockfd == -1)
    {
        free(host);
        free(request);
        fprintf(stderr, "creating socket failed\n");
        exit(EXIT_FAILURE);
    }

    FILE *w_file = output_file(request, file, dir);
    if(w_file == NULL)
    {   
        free(host);
        free(request);
        ERROR("opening output file failed", EXIT_FAILURE);
    }

    FILE *sockfile = fdopen(sockfd, "r+");
    if(sockfile == NULL)
    {   
        free(host);
        free(request);
        fclose(w_file);
        ERROR("opening socket fd failed", EXIT_FAILURE);
    }

    send_request(request, host, sockfile);

    int h = validate_header(sockfile);
    if (h != 0)
    {
        free(host);
        free(request);
        exit(h);
    }

    transmit_content(sockfile, w_file);

    fclose(sockfile);
    fclose(w_file);
    free(host);
    free(request);

    return EXIT_SUCCESS;
}