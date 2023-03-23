/*
 * tcping.c
 *
 * Copyright (c) 2002-2023 Marc Kirchner
 *
 * tcping does a nonblocking connect to test if a port is reachable.
 * Its exit codes are:
 *     -1 an error occured
 *     0  port is open
 *     1  port is closed
 *     2  user timeout
 */

#define VERSION 2.0.0

#define log(verbosity, fmt, ...)                                               \
    do {                                                                       \
        if (verbosity)                                                         \
            fprintf(fmt, __VA_ARGS__);                                         \
    } while (0)

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

void usage();

int main(int argc, char *argv[])
{

    int sockfd;
    struct sockaddr_in addr;
    struct hostent *host;
    int error = 0;
    int ret;
    socklen_t errlen;
    struct timeval timeout;
    fd_set fdrset, fdwset;
    int verbosity = 1;
    int c;
    char *cptr;
    long timeout_sec = 0, timeout_usec = 0;
    int port = 0;

    if (argc < 3) {
        usage(argv[0]);
    }

    while ((c = getopt(argc, argv, "qt:u:")) != -1) {
        switch (c) {
        case 'q':
            verbosity = 0;
            break;
        case 't':
            cptr = NULL;
            timeout_sec = strtol(optarg, &cptr, 10);
            if (cptr == optarg)
                usage(argv[0]);
            break;
        case 'u':
            cptr = NULL;
            timeout_usec = strtol(optarg, &cptr, 10);
            if (cptr == optarg)
                usage(argv[0]);
            break;
        default:
            usage(argv[0]);
            break;
        }
    }

    memset(&addr, 0, sizeof(addr));

    if ((host = gethostbyname(argv[optind])) == NULL) {
        log(verbosity, stderr, "error: %s\n", hstrerror(h_errno));
        exit(-1);
    }

    memcpy(&addr.sin_addr, host->h_addr_list[0], host->h_length);
    addr.sin_family = host->h_addrtype; /* always AF_INET */
    if (argv[optind + 1]) {
        cptr = NULL;
        port = strtol(argv[optind + 1], &cptr, 10);
        if (cptr == argv[optind + 1])
            usage(argv[0]);
    } else {
        usage(argv[0]);
    }
    addr.sin_port = htons(port);

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    fcntl(sockfd, F_SETFL, O_NONBLOCK);
    if ((ret = connect(sockfd, (struct sockaddr *)&addr, sizeof(addr))) != 0) {
        if (errno != EINPROGRESS) {
            log(verbosity, stderr, "error: %s port %s: %s\n", argv[optind],
                argv[optind + 1], strerror(errno));
            return (-1);
        }

        FD_ZERO(&fdrset);
        FD_SET(sockfd, &fdrset);
        fdwset = fdrset;

        timeout.tv_sec = timeout_sec + timeout_usec / 1000000;
        timeout.tv_usec = timeout_usec % 1000000;

        if ((ret = select(sockfd + 1, &fdrset, &fdwset, NULL,
                          timeout.tv_sec + timeout.tv_usec > 0 ? &timeout
                                                               : NULL)) == 0) {
            /* timeout */
            close(sockfd);
            log(verbosity, stdout, "%s port %s user timeout.\n", argv[optind],
                argv[optind + 1]);
            return (2);
        }
        if (FD_ISSET(sockfd, &fdrset) || FD_ISSET(sockfd, &fdwset)) {
            errlen = sizeof(error);
            if ((ret = getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &error,
                                  &errlen)) != 0) {
                /* getsockopt error */
                log(verbosity, stderr, "error: %s port %s: getsockopt: %s\n",
                    argv[optind], argv[optind + 1], strerror(errno));
                close(sockfd);
                return (-1);
            }
            if (error != 0) {
                log(verbosity, stdout, "%s port %s closed.\n", argv[optind],
                    argv[optind + 1]);
                close(sockfd);
                return (1);
            }
        } else {
            log(verbosity, stderr, "error: select: sockfd not set\n");
            exit(-1);
        }
    }
    /* OK, connection established */
    close(sockfd);
    log(verbosity, stdout, "%s port %s open.\n", argv[optind],
        argv[optind + 1]);
    return 0;
}

void usage(char *prog)
{
    fprintf(stderr,
            "error: Usage: %s [-q] [-t timeout_sec] [-u timeout_usec] <host> "
            "<port>\n",
            prog);
    exit(-1);
}
