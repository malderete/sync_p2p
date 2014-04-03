#ifndef SESSIONS_H_
#define SESSIONS_H_

typedef struct session_struct Session;

struct session_struct {
    int fd;
    char* ip;
    char* filename;
    unsigned int total;
    unsigned int offset;
};

typedef struct session_list_struct SessionList;

struct session_list_struct {
    Session* session;
    SessionList* next;
};


Session* create_session();
SessionList* add_session(SessionList* list, Session* session);
Session* get_session(SessionList *list , int fd_expected);
SessionList* remove_session(SessionList* list , int fd_expected);


#endif /* SESSIONS_H_ */
