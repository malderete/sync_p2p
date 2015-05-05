#ifndef SERVER_H_
#define SERVER_H_

#include <netinet/in.h> //uint16_t
#include "sessions.h"

// Extras
#define BACKLOG 10 // Cuantas conexiones permitimos en la cola de pendientes
#define SERVER_STATUS_ACTIVE 1
#define SERVER_STATUS_INACTIVE 0
#define KNOWN_NODE_ACTIVE 1
#define KNOWN_NODE_INACTIVE 0


typedef struct known_node_struct KnownNode;
typedef struct server_struct Server;

struct known_node_struct {
    char* hostname;
    char* ip;
    unsigned int port;
    unsigned int status;
};

struct server_struct {
    char *name;
    int server_fd;
    char *server_port;
    char *share_directory;
    char *download_directory;
    int status;
    char *files;
    KnownNode** known_nodes;
    unsigned int known_nodes_length;
};


int server_get_port_for_active_node(char *ip);
int server_set_node_as_active(char *ip);
int server_send_file_info(Session *session, char *filename);
int server_send_file_segment(Session *session, char *filename);
void handle_message(int sd, uint16_t message_code, char * message);
int server_init_stack(void);


#endif /* SERVER_H_ */
