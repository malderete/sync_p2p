#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h> //uint16_t
#include <sys/types.h>
#include <sys/socket.h>


#include "include/protocol.h"


/*
* Lee n bytes de un fd y los almacena en buffer
* Retorna la cantidad de bytes leidos
*/
int read_n_bytes(int sd, void *buffer, int n) {
    int dev;
    int done;

    dev   = 1;
    done = 0;
    while ((done < n) && (dev != 0)) {
        dev = recv(sd, buffer+done, (n-done) , 0);
        if (dev < 0) {
             perror("read_n_bytes recv");
             exit(EXIT_FAILURE);
        }
        done += dev;
    }
    return done;
}


/*
* Leemos un 'mensaje' de nuestro protocolo
* basicamente es el parser del mensaje
* Retorna la cantidad de bytes leidos
*/
int read_message(int sd, uint16_t * code, char *message) {
    int n;
    uint16_t lon, message_code, one_byte;

    // Vemos si en el buffer del SO hay algo disponible
    // sin bloquear (MSG_DONTWAIT) pero no sacamos nada del buffer
    // (MSG_PEEK)
    n = recv(sd, &one_byte, 1, MSG_DONTWAIT | MSG_PEEK);
    if (n == 0) {
        return 0;
    }

    n = -1; // Algo que indica que no leimos...
    n = read_n_bytes(sd, &message_code, HEADER_CODE_LENGTH);
    message_code = ntohs(message_code);
    *code = message_code;

    n = read_n_bytes(sd, &lon, HEADER_SIZE_LENGTH);
    lon = ntohs(lon);

    if (n != 0) {
        n = read_n_bytes(sd, message, lon);
    }

    return n;
}


/*
* Enviamos un 'mensaje' de nuestro protocolo
* Retorna la cantidad de bytes enviados
*/
int send_message(int sd, uint16_t code, char *message) {
    int n;
    uint16_t lon;
    uint16_t lon_nbo;
    char buffer[HEADER_CODE_LENGTH+HEADER_SIZE_LENGTH+MAX_SIZE];

    lon = strlen(message) + 1;
    lon_nbo = htons(lon);

    code = htons(code);
    memcpy(buffer, &code , HEADER_CODE_LENGTH);
    memcpy(buffer + HEADER_CODE_LENGTH , &lon_nbo , HEADER_SIZE_LENGTH);
    memcpy(buffer + HEADER_CODE_LENGTH + HEADER_SIZE_LENGTH, message, lon);

    // Esto no me convonce, ponerlo en un WHILE!
    n = send(sd, buffer, (HEADER_CODE_LENGTH + HEADER_SIZE_LENGTH + lon), 0);

    return n;
}
