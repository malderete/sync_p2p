#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>


#include "include/main.h"
#include "include/server.h"
#include "include/protocol.h"
#include "include/tasks.h"


extern Server server;
extern int pipe_fds[2];
extern char pipe_buf[PIPE_SIZE];


/*
* Esta funcion representa un thread WORKER
* encargado de descargar un archivo de un nodo
* debe tomar los trabajos de una cola
*/
void downloader_worker_thread(void *worker_data) {
    // get mutex
    // while(!condition()) {
    //
    // }
}


/*
* Intenta conectarse a cada uno de los nodos conocidos
* para intercambiar la lista de archivos
* Este caso se da cuando este nodo no es el primero en
* iniciar.
*
*/
void client_broadcast_nodes() {
    int sd_node, node_size, i;
    //uint16_t code;
    struct sockaddr_in node;

    // Seteamos el puerto y la familia igual para
    // todos los nodos
    node_size = sizeof(node);
    node.sin_family = AF_INET;
    node.sin_port = htons(CLIENT_PORT); //main.h

    //code = REQUEST_LIST;
    printf("Buscando nodos ACTIVOS...\n");
    for(i=0; i < 3; i++) {
        sd_node = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
        // Seteo la ip del otro nodo
    	node.sin_addr.s_addr = inet_addr(server.known_clients[i]);
    	//Intentamos conectarnos para ver si esta vivo!
        if (connect(sd_node, (struct sockaddr*)&node, node_size) >= 0) {
            printf("nodo ACTIVO: %s Intercambiando lista de archivos\n", server.known_clients[i]);
            server.actives_clients[i] = 1;
            //send_message(sd_node, code, "hola;chau;");
            // Desconecto
            close(sd_node);
        } else {
            printf("nodo INACTIVO: %s\n", server.known_clients[i]);
        }
    }
}


/*
* Inicia el bucle principal del proceso HIJO
* que planifica las descargas
*/
void downloader_init_stack(void) {
    int done;

    // Iniciamos las tareas
    tasks_initialize();
    // Busquemos nodos prendidos...
    client_broadcast_nodes();
    // Iniciar N thread con downloader_worker_thread()
    // start_download_workers();
    done = 0;
    printf("Downloader Scheduler esperando datos...\n");
    // Crear N workers
    while(!done) {
        read(pipe_fds[0], pipe_buf, PIPE_SIZE);
        fprintf(stderr, "HIJO Leido %s (%lu bytes)\n", pipe_buf, strlen(pipe_buf));
        if (strncmp(pipe_buf, STOP_MESSAGE, 3) == 0) {
        	done = 1;
        	printf("%s\n", "HIJO Terminando...");
        } else {
        	task_parse_list_message(pipe_buf);
        	// Emitir una SIGNAL (pthread_cond_broadcast(mutex, cond))
        	// para despertar los workers
        }

    }
    //DEBUG
/*
    Task* t = task_get();
    while(t != NULL) {
    	printf("IP: %s\n", t->ip);
    	printf("Filename: %s\n", t->filename);
    	printf("Total: %d\n\n", t->total);
    	t = task_get();
    }
*/
    exit(0);
}
