#include <stdio.h>
#include <stdlib.h>

#include "include/sessions.h"

// Definimos como alocar!
#define SESSIONLIST_NEW() (SessionList *)malloc(sizeof(SessionList))
#define SESSION_NEW() (Session *)malloc(sizeof(Session))


/*
* Crea una session
*/
Session* create_session() {
    Session* new;
    // alocamos memoria
    new = SESSION_NEW();
    // Iniciamos los campos
    new->fd = -1;
    new->ip = "";
    new->filename = "";
    new->offset = 0;
    new->total = 0;

    return new;
}

/*
* Agrega una session al inicio de la lista
*/
SessionList* add_session(SessionList* list, Session* session) {
    SessionList* new;
    new = SESSIONLIST_NEW();
    new->session = session;
    new->next = list;
    list = new;
    return list;
}

/*
* Devuelve la session cuyo fd coindide con fd_expected
*/
Session* get_session(SessionList *list , int fd_expected) {
    SessionList* aux;
    aux = list;

    while (aux->session->fd != fd_expected) {
        aux = aux->next;
    }
    return aux->session;
}

/*
* Elimina la session cuyo fd coindide con fd_expected
*/
SessionList* remove_session(SessionList* list , int fd_expected) {
    SessionList* current , *before;
    current = list;
    before = list;
    int current_val;
    Session* s;

    s = current->session;
    current_val = s->fd;
    while ( current_val != fd_expected){
        before = current;
        current = current->next;
        s = current->session;
        current_val = s->fd;
    }
    if (list == current) {
        if ( current->next == NULL ) {
            list = NULL;
        } else {
            list = current->next;
        }
    } else {
        before->next = current->next;
    }
    free (current->session);
    free(current);
    return list;
}
