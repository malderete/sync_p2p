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
#include "include/dns.h"
#include "include/file_system.h"
#include "include/ipc_protocol.h"
#include "include/server.h"
#include "include/sessions.h"
#include "include/signals.h"
#include "include/parse_file.h"

#include <jansson.h>

// Definimos como alocar!
#define KNOWN_NODE_LIST_NEW(size) calloc(size, sizeof(KnownNode *));
#define KNOWN_NODE_NEW() (KnownNode*)malloc(sizeof(KnownNode))


// FD del pipe
// Leer de pipe_fds[0]
// Escribir en pipe_fds[1]
int pipe_fds[2];


// VARIABLES GLOBALES!!!
Server server;
SessionList* sessions = NULL;
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
    json_t *main_json;
    json_error_t error;

    main_json = json_load_file(config_filename, 0, &error);
    if(!main_json) {
        printf("error: en linea %d: %s\n", error.line, error.text);
        return 1;
    }
    if(!json_is_object(main_json)) {
        printf("error: json no es un objeto\n");
        json_decref(main_json);
        return 1;
    }

    int i, size;
    char* final_ip;
    json_t *json;

    json = json_object_get(main_json, "name");
    if (!json_is_string(json)) {
        printf("error: 'name' no es un string\n");
        json_decref(json);
        return 1;
    }
    // Seteamos el name de nuestro server
    server.name = strdup(json_string_value(json));

    json = json_object_get(main_json, "port");
    if (!json_is_string(json)) {
        printf("error: 'port' no es un string\n");
        json_decref(json);
        return 1;
    }
    // Seteamos el puerto donde escuchara el servidor
    server.server_port = strdup(json_string_value(json));

    json = json_object_get(main_json, "share_dir");
    if (!json_is_string(json)) {
        printf("error: 'share_dir' no es un string\n");
        json_decref(json);
        return 1;
    }
    // Seteamos el directorio compatido
    server.share_directory = strdup(json_string_value(json));


    json = json_object_get(main_json, "download_dir");
    if (!json_is_string(json)) {
        printf("error: 'download_dir' no es un string\n");
        json_decref(json);
        return 1;
    }
    // Seteamos el directorio donde descargar
    server.download_directory = strdup(json_string_value(json));

    json = json_object_get(main_json, "nodes");
    if (!json_is_array(json)) {
        printf("error: 'nodes' no es un array\n");
        json_decref(json);
        return 1;
    }
    size = json_array_size(json);

    // Iniciamos los nodos conocidos por el server
    server.known_nodes = KNOWN_NODE_LIST_NEW(size);
    server.known_nodes_length = size;

    for(i = 0; i < size; i++) {
        json_t *data, *ip, *port;
        const char* ip_text;
        const char* port_text;

        data = json_array_get(json, i);
        if(!json_is_object(data)) {
            printf("error: 'nodes[%d]' no es un objeto\n", i + 1);
            json_decref(json);
            return 1;
        }

        ip = json_object_get(data, "ip");
        if(!json_is_string(ip)) {
            printf("error: 'nodes[%d].ip' no es un string\n", i + 1);
            json_decref(json);
            return 1;
        }

        port = json_object_get(data, "port");
        if(!json_is_string(ip)) {
            printf("error: 'nodes[%d].port' no es un string\n", i + 1);
            json_decref(json);
            return 1;
        }

        ip_text = json_string_value(ip);
        port_text = json_string_value(port);

        // Consulta DNS para optener la IP
        final_ip = dns_getip((char *)ip_text);
        if (!final_ip) {
            printf("error: dns_getip(%s) fallo\n", ip_text);
            return 1;
        }
        // Agrego el KnownNode al server
        KnownNode* known_node;
        known_node = KNOWN_NODE_NEW();
        known_node->hostname = strdup(ip_text);
        known_node->ip = strdup(final_ip);
        known_node->port = atoi(port_text);
        known_node->status = KNOWN_NODE_INACTIVE;
        server.known_nodes[i] = known_node;
    }
    json_decref(main_json);
    return 0;
}


int
main(int argc, char** argv) {
    pid_t pid;

    // Llamamos a pipe() antes del fork para que el hijo herede los FD abiertos por pipe() ;)
    // Este es un mecanismo de IPC para comunicar los 2 procesos
    // pipe_fds es un array de 2 posiciones en la 0 se lee en la 1 se escribe
    pipe(pipe_fds);

    if (parse_arguments(argc, argv) == -1) {
        fprintf(stderr, "[!] Error parseando argumentos.\n");
        return EXIT_FAILURE;
    }
    if (config_load(config_filename) == 1) {
        fprintf(stderr, "[!] Error parseando archivo de configuracion.\n");
        return EXIT_FAILURE;
    }
    // Hacemos que el CHILD no maneje KILL ;)
    if (signals_disable() == -1) {
        fprintf(stderr, "[!] Error desabilitando los manejadores de signals.\n");
        return EXIT_FAILURE;
    }
    printf(
            "[*] Configuracion utilizada:\n"
            "    Server name: %s\n"
            "    Server port: %s\n"
            "    Share dir: %s\n"
            "    Download dir: %s\n\n",
            server.name, server.server_port,
            server.share_directory, server.download_directory
    );
    //const char* directory = CONFIG_DEFAULT_SERVER_DIR;
    //filesystem_load(directory);
    filesystem_load(server.share_directory);

    server.status = SERVER_STATUS_INACTIVE;

    pid = fork();
    if (pid == -1) {
        perror("fork");
        return EXIT_FAILURE;
    }

    // El Parent es un server single-thread que atiende clientes
    // El Child es un proceso que planifica threads con las descargas
    // Para comunicar los dos usamos un pipe unidireccional (server -> client)

    if (pid == 0) { //CHILD
         printf(
                 "[*] HIJO Creado, iniciando cliente (PID=%d):\n"
                 " Esperando datos en el PIPE...\n", getpid()
        );
         // Cerramos el fd del pipe para escribir
         close(pipe_fds[1]);
         downloader_init_stack();
    } else { //PARENT
        if (signals_initialize() == -1) {
            fprintf(stderr, "[!] Error iniciando los manejadores de signals.\n");
            ipc_send_message(pipe_fds[1], IPC_STOP_MESSAGE);
            return EXIT_FAILURE;
        }
        printf("[*] Iniciando server(PID=%d)\n", getpid());
        if (server_init_stack() == -1) {
            fprintf(stderr, "[!] Problemas al iniciar el servidor\n");
            ipc_send_message(pipe_fds[1], IPC_STOP_MESSAGE);
        }
        // Antes de salir esperamos al otro hijo!
        waitpid(pid, NULL, 0);
    }
    return EXIT_SUCCESS;
}
