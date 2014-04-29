#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>


#include "include/client.h"
#include "include/file_system.h"
#include "include/main.h"
#include "include/server.h"
#include "include/sessions.h"
#include "include/signals.h"
#include "include/parse_file.h"

#include <jansson.h>

// FD del pipe
// Leer de pipe_fds[0]
// escribir de pipe_fds[1]
int pipe_fds[2];
char pipe_buf[PIPE_SIZE];


// VARIABLES GLOBALES!!!
Server server;
SessionList* sessions = NULL;
//extern Config config;
char* config_filename;


static void
show_help(char** argv) {
    printf(
        "IW Sync_P2P Server v 0.1\n\n"
        "  -h            Muestra esta ayuda.\n"
        "  -c <archivo>  Especifica el archivo de configuracion (Path absoluto).\n"
        "Uso:\n"
        "   %s -c file.json\n",
        argv[0]
    );
    exit(EXIT_SUCCESS);
}


static int
parse_arguments(int argc, char** argv) {
	int c;
	while((c = getopt(argc, argv, "hc:")) != -1) {
		switch(c) {
			case 'h':
				show_help(argv);
				break;
			case 'c':
				config_filename = strdup(optarg);
				fprintf(stdout, "[*] Archivo de configuracion: %s\n", config_filename);
				break;
			case '?':
				fprintf(stderr, "[!] Opcion invalida\n");
				return -1;
			default:
				return -1;
		}
	}
	return 0;
}


int
config_load(char* config_filename) {
    json_t *json;
    json_error_t error;

    json = json_load_file(config_filename, 0, &error);
    if(!json) {
    	printf("error: on line %d: %s\n", error.line, error.text);
    	return 1;
    }
    if(!json_is_array(json)) {
        printf("error: json is not an array\n");
        json_decref(json);
        return 1;
    }

    int i, size;
    size = json_array_size(json);
    if (size > 10) {
    	size = 10;
    }
    for(i = 0; i < size; i++) {
    	json_t *data, *ip, *port;
    	const char* ip_text;
    	const char* port_text;

    	data = json_array_get(json, i);
    	if(!json_is_object(data)) {
        	printf("error: commit data %d is not an object\n", i + 1);
        	json_decref(json);
        	return 1;
    	}

    	ip = json_object_get(data, "ip");
    	if(!json_is_string(ip)) {
        	printf("error: data %d: ip is not a string\n", i + 1);
        	json_decref(json);
        	return 1;
    	}
    	port = json_object_get(data, "port");
    	if(!json_is_string(ip)) {
        	printf("error: data %d: port is not a string\n", i + 1);
        	json_decref(json);
        	return 1;
    	}
    	ip_text = json_string_value(ip);
    	port_text = json_string_value(port);
    	server.known_clients[i] = strdup(ip_text);
    }
    json_decref(json);
    return 0;
}


int
main(int argc, char** argv) {
    pid_t pid;

    // Inicializamos el chunk de memoria
    memset(pipe_buf, 0, PIPE_SIZE);

    // Llamamos a pipe() antes del fork para que el hijo herede los FD abiertos por pipe() ;)
    // Este es un mecanismo de IPC para comunicar los 2 procesos
    // pipe_fds es un array de 2 posiciones en la 0 se lee en la 1 se escribe
    pipe(pipe_fds);

    /*TODO: Antes del fork() tenemos que
         config_load() donde cargamos toda la conf y alguna magia...
         sygnals_initialize() Manejador de signals
         server_parse_arguments(argc, argv) (Tenemos?)
         Iniciar el file_system_reader() (?)
    */
    if (parse_arguments(argc, argv) == -1) {
    	fprintf(stderr, "[!] Error parseando argumentos.\n");
    	return EXIT_FAILURE;
    }
    //config_filename = CONFIG_FILE_PATH;
    if (config_load(config_filename) == 1) {
    	fprintf(stderr, "[!] Error parseando archivo de configuracion.\n");
    	return EXIT_FAILURE;
    }
    if (signals_initialize() == -1) {
    	fprintf(stderr, "[!] Error iniciando los manejadores de signals.\n");
    	return EXIT_FAILURE;
    }
    const char* directory = CONFIG_DEFAULT_SERVER_DIR;
    filesystem_load(directory);

    server.name = "IW Test Server";
    server.status = SERVER_STATUS_INACTIVE;
    //TODO:  Hacer esto configurable!
    //server.known_clients[0] = "127.0.0.1";
    //server.known_clients[1] = "192.168.1.102";
    //server.known_clients[2] = "192.168.1.13";

    pid = fork();
    if (pid == -1) {
        perror("fork");
        return EXIT_FAILURE;
    }

    // El Parent es un server single-thread que atiende clientes
    // El Child es un proceso que planifica threads con las descargas
    // Para comunicar los dos usamos un pipe.

    if (pid == 0) { //CHILD
         printf(
        		 "[*] HIJO Creado, iniciando cliente (PID=%d)\n:"
        		 " Esperando datos en el PIPE...\n", getpid()
        );
         // Cerramos el fd del pipe para escribir
         close(pipe_fds[1]);
         downloader_init_stack();
    } else { //PARENT
        printf("[*] Iniciando server(PID=%d)\n", getpid());
        if (server_init_stack() == -1) {
            fprintf(stderr, "[!] Problemas al iniciar el servidor\n");
	        //TODO: Avisar al hijo que algo salio mal(?)
        }
        // Antes de salir esperamos al otro hijo!
        waitpid(pid, NULL, 0);
        //server_shutdown();
    }
    return EXIT_SUCCESS;
}
