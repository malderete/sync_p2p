#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>


/*
 * Realiza una consulta DNS para optener
 * la IP asociada al hostname dado.
 */
char *dns_getip(char *hostname) {
    struct hostent *hostinfo;
    char *ip;

    ip = NULL;
    hostinfo = gethostbyname(hostname);

    if (hostinfo) {
        ip = inet_ntoa(*(struct in_addr *)hostinfo->h_addr);
    }
    return ip;
}
