#include <arpa/inet.h>
#include <unistd.h>
#include <netinet/in.h> //uint16_t
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "include/ipc_protocol.h"


int ipc_send_message(int fd, char *message) {
    int nbytes;
    short size;
    char *msg;

    msg = (char*)malloc(strlen(message) + sizeof(size));
    size = sizeof(char) + strlen(message);

    memcpy(msg, &size, sizeof(size));
    memcpy(msg + sizeof(size), message, strlen(message));


    nbytes = write(fd, msg, sizeof(size) + strlen(message));

    return nbytes;
}

int ipc_read_message(int fd, char **message) {
    int nbytes;
    short to_read;

    read(fd, &to_read, sizeof(to_read));
    //printf("protocolo tamaño: %u\n", to_read);

    *message = (char*)malloc(to_read);
    nbytes = read(fd, *message, to_read);
    //printf("Protocolo msg: %s\n", *message);

    return nbytes;
}

