#ifndef MAIN_H_
#define MAIN_H_

#define PIPE_SIZE 1024 // tamanio para leer/escribir en el PIPE
#define CLIENT_PORT 4444
#define CONFIG_DEFAULT_SERVER_DIR "/home/tincho/proyectos/iw/"
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
	int server_port;
	int status;
	char *files;
	KnownNode** known_nodes;
	unsigned int known_nodes_length;
};

#endif /* MAIN_H_ */
