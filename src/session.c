/*
GPL LICENSE
-----------

Copyright 2012 Martin Alderete <malderete@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
MA 02110-1301, USA.
*/

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <assert.h>

#include "include/session.h"

//Array of session using FD as index
static Session **sessions;
//Current index
static size_t current_sessions = 0;
static size_t max_sessions = 0;

//void func( void (*f)(int) );


int
session_initialize(size_t sessions_max) {
    //Set the limit of session allowed!
    max_sessions = sessions_max;
    sessions = calloc(max_sessions, sizeof(Session *));

    if(sessions == NULL) {
		perror("session_initialize():");
		return -1;
    }
    return 0;
}

int
session_add(Session *session) {
    int fd = session->connection_fd;

    if(sessions[fd]) {
        fprintf(stderr, "session_add(fd=%d): Error, session existente\n", fd);
        return -1;
    }
    sessions[fd] = session;
    return 0;
}

static int
session_remove(Session *session) {
    int fd = session->connection_fd;

    if((size_t) fd > max_sessions || !sessions[fd]) {
        fprintf(stderr, "session_remove(fd=%d): Error, session inexistente\n", fd);
        return -1;
    }
    sessions[fd] = NULL;
    return 0;
}

Session *
session_get(int fd) {
    return ((size_t) fd > max_sessions) ? NULL : sessions[fd];
}

int
session_create(Session **session, int connection) {
    if(current_sessions == max_sessions)
    {
        fprintf(stderr, "session_create(): Se llego al limite de sessiones\n");
        return -1;
    }

    Session *tmp = malloc(sizeof(Session *));
    if(!tmp) {
        perror("session_create():");
        return -1;
    }

    tmp->connection_fd = connection;
    tmp->id = connection;
    tmp->filename = "";
    tmp->total = (long)0;
    tmp->offset = 0;
    tmp->status = SESSION_WAITING;

    *session = tmp;
    current_sessions++;
    
    return session_add(*session);
}

int
session_destroy(Session * session) {
    int ret = session_remove(session);

    assert(session != NULL);

    free(session);
    current_sessions--;

    return ret;
}

int
session_finalize() {
    free(sessions);
    return 0;
}

void
session_show() {
    int i;
    for(i=0; i<current_sessions; i++) {
        fprintf(stdout, "Index=%d  ID=%d\n", sessions[i]->connection_fd,
        sessions[i]->id);
    }
}
