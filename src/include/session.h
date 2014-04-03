#ifndef SESSION_H
#define SESSION_H

#include <sys/types.h>

#define SESSION_WAITING  0
#define SESSION_ACTIVE   1
#define SESSION_STARTING 2

typedef struct session_struct Session;

struct session_struct {
    int connection_fd;    // socket con la session
    unsigned int id;    // identificador unico es el indice dentro del array de sessiones
    char * filename;    // archivo a descargar
    long total; // total en bytes
    unsigned int offset;    // offset dentro del archivo a descargar
    int status;        // status del sessione
};


/*Initialize the session system*/
int session_initialize(size_t max_sessions);
/*Create a session*/
int session_create(Session **session, int connection);
/*Destroy a session*/
int session_destroy(Session * session);
/*Get a session*/
Session * session_get(int id);

#endif
