#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <errno.h>
#include <unistd.h>
#include "arraylist.h"

#define BUFSIZE 1024

struct myargs {
    char* url;
    char domain[BUFSIZE]; // Domain of URL
    char path[BUFSIZE]; // Path of URL
    char* port; // 80 by default
    char* target;
    struct timeval timeout; // 10 (second) by default
};

/**
 * @brief Separate a URL out into the domain part and the path part
 * 
 * @param url Pointer to a URL string
 * @param domain Pointer to a domain string to which to write result, 
 *               assumed to be BUFSIZE large
 * @param path Pointer to path string to which to write result,
 *               assumed to be BUFSIZE large.  If there is no path, put
 *               a "/"
 */
void parseURL(char* url, char* domain, char* path) {
    char* httpStr = "http://";
    int len = strlen(url);
    memset(domain, '\0', BUFSIZE);
    memset(path, '\0', BUFSIZE);
    if (strncmp(url, httpStr, strlen(httpStr)) == 0) {
        // Skip over any http:// at the front
        url += strlen(httpStr);
    }
    int idxSep = 0;
    while (idxSep < len && url[idxSep] != '/') {
        idxSep++;
    }

    if (idxSep+1 > BUFSIZE) {
        fprintf(stderr, "ERROR: Domain part of URL exceeds %i bytes", BUFSIZE);
        exit(0);
    }
    strncpy(domain, url, idxSep);

    if (len-idxSep+1 > BUFSIZE) {
        fprintf(stderr, "ERROR: Path part of URL exceeds %i bytes", BUFSIZE);
        exit(0);
    }
    if (idxSep == len) {
        // No path specified; default to "/"
        path[0] = '/';
    }
    else {
        strncpy(path, url+idxSep, len-idxSep+1);
    }
}

/**
 * @brief Parse command line arguments for the HTTP client
 */
struct myargs parseArgs(int argc, char** argv) {
    struct myargs ret;
    // Step 1: Setup default values
    ret.url = "";
    ret.port = "80";
    ret.timeout.tv_sec = 10;
    ret.timeout.tv_usec = 0;
    ret.target = "";

    // Step 2: Parse user specified values
    // Advance to the next element
    char* programName = argv[0];
    argv++; 
    argc--; 
    while (argc > 0) {
        if((*argv)[0] == '-') {
            if (strcmp(*argv, "--help") == 0) {
                printf("Usage: %s --url <url of file>", programName);
                printf(" --target <target filename to save>");
                printf(" [--port <port number>] [--timeout <timeout>]\n");
                exit(0);
            }
            else if (strcmp(*argv, "--url") == 0) {
                argv++; argc--;
                if (argc > 0) {
                    ret.url = *argv;
                    parseURL(*argv, ret.domain, ret.path);
                }
                else {
                    fprintf(stderr, "Error: Expecting field after --url\n");
                    exit(0);
                }
            }
            else if (strcmp(*argv, "--port") == 0) {
                argv++; argc--;
                if (argc > 0) {
                    ret.port = *argv;
                }
                else {
                    fprintf(stderr, "Error: Expecting field after --port\n");
                    exit(0);
                }
            }
            else if (strcmp(*argv, "--target") == 0) {
                argv++; argc--;
                if (argc > 0) {
                    ret.target = *argv;
                }
                else {
                    fprintf(stderr, "Error: Expecting field after --path\n");
                    exit(0);
                }
            }
            else if (strcmp(*argv, "--timeout") == 0) {
                argv++; argc--;
                if (argc > 0) {
                    ret.timeout.tv_sec = atol(*argv);
                }
                else {
                    fprintf(stderr, "Error: Expecting field after --timeout\n");
                    exit(0);
                }
            }

        }
        else {
            fprintf(stderr, "Warning: Unrecognized field %s\n", *argv);
        }
        argv++; argc--;
    }

    // Step 3: Check for required values
    if (strcmp(ret.url, "") == 0) {
        fprintf(stderr, "Error: Require a --url to be specified\n");
        exit(0);
    }
    if (strcmp(ret.target, "") == 0) {
        fprintf(stderr, "Error: Require a --target to be specified\n");
        exit(0);
    }

    return ret;
}


int main(int argc, char** argv) {
    struct myargs args = parseArgs(argc, argv);

    //TEST
    printf("Domain: %s\n", args.domain);
    printf("Port: %s\n", args.port);
    
    // TODO: Fill this in.  You may want to add some helper methods for better organization
    /* PART 1 */
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;    /* Allow IPv4 or IPv6 */
    hints.ai_socktype = SOCK_STREAM; // 2-way byte stream; Works like a pipe
    hints.ai_flags = 0;
    hints.ai_protocol = 0;          /* Any protocol */

    struct addrinfo* node;  // Points to linked list of addrinfo

    int addrInfo = getaddrinfo(args.domain, args.port, &hints, &node);
    if(addrInfo != 0) { 
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(addrInfo));
        exit(EXIT_FAILURE);
    }
    else { printf("getaddrinfo() SUCCESS!\n"); }    // TEST

    int sfd;
    for (struct addrinfo *rp = node; rp != NULL; rp = rp->ai_next) {
        sfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sfd == -1)
            continue;

        if (connect(sfd, rp->ai_addr, rp->ai_addrlen) != -1) {
            printf("Successfully connected to domain!\n");
            break;
        }
    }

    if (sfd == -1) {
        fprintf(stderr, "No socket found");
        exit(EXIT_FAILURE);
    }

    /* PART 2 */
    // HTML GET Request Example?
    // GET /ctralie_cv.pdf HTTP/1.0
    // Connection: close

    char getReq[BUFSIZE];
    
    int requestLen = snprintf(getReq, BUFSIZE, 
                              "GET %s HTTP/1.0\r\n"
                              "Host: %s\r\n"
                              "Connection: close\r\n"
                              "\r\n",
                              args.path, args.domain);

    printf("%s", getReq);

    int totalSent = 0;

    while (totalSent < requestLen) {
        ssize_t sent = send(sfd, getReq + totalSent, requestLen - totalSent, 0);

        if (sent < 0) {
            perror("send");
            close(sfd);
            freeaddrinfo(node);
            exit(EXIT_FAILURE);
        }

        totalSent += sent;
    }

    printf("Sent %d bytes\n", totalSent);

    /* PART 3 */

    char receiveMssg[BUFSIZE];
    ssize_t receiveStatus;
    struct ArrayListBuf mybuf;
    ArrayListBuf_init(&mybuf);  // Create ArrayListBuf
    char tempBuff[BUFSIZE];

    FILE *file = fopen(args.target, "w");

    if(file == NULL) {
        printf("Error opening the file!\n");
        return 1;
    }

    while ((receiveStatus = recv(sfd, receiveMssg, BUFSIZE, 0)) > 0) {
        printf("Received %zd bytes\n", receiveStatus); // %zd for ssize_t vars; ssize_t represents a size of an allocated block of memory (along with -1 for errors)

        // Store these bytes in the ArrayList
        ArrayListBuf_push(&mybuf, receiveMssg, receiveStatus);

        // Get first line
        snprintf(tempBuff, BUFSIZE, "%s", strtok(mybuf.buff, "\n"));
            
        // Check if status is 200 OK
        printf("tempBuff: %s\n", tempBuff); // TEST
        if (strstr(tempBuff, "200 OK") == NULL) {
            printf("Server sent wrong status: %s", tempBuff);
            exit(EXIT_FAILURE);
        }
        // Write data to specified target file as binary data w/ fwrite()
        
        // Search for body
        while(snprintf(tempBuff, BUFSIZE, "%s", strtok(NULL, "\n")) > 0) {
            if (strcmp(tempBuff, "\r\n")) {
                printf("Reading data into file...\n");
                break;
            }
        }

        // Reads until the end of the body
        snprintf(tempBuff, BUFSIZE, "%s", strtok(NULL, "\0"));
        fputs(tempBuff, file);
    }

    if (receiveStatus < 0) {
        perror("recv");
    }
    else {
        printf("Server closed the connection.\n");
    }

    fclose(file);
    freeaddrinfo(node);
    close(sfd);
    ArrayListBuf_free(&mybuf);

    exit(EXIT_SUCCESS);
}
