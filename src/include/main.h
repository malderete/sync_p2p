#ifndef MAIN_H_
#define MAIN_H_

#define PIPE_SIZE 1024 // tamanio para leer/escribir en el PIPE
#define CLIENT_PORT 4444
#define CONFIG_FILE_PATH "/home/tincho/config.json"
#define CONFIG_DEFAULT_SERVER_DIR "/home/tincho/proyectos/iw/"

#define SERVER_STATUS_ACTIVE 0
#define SERVER_STATUS_INACTIVE 1

typedef struct server_struct Server;
struct server_struct {
	char *name;
	int server_fd;
	int server_port;
	char* known_clients[3];
	int actives_clients[3];
	int status;
	char *files;
};



/*typedef struct config_struct Config;
struct config_struct {
	int server_udp_port;
	int server_tcp_port;
};*/


#endif /* MAIN_H_ */
