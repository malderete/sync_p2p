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
    int sd_known_node, sockadd_size, i;
    //uint16_t code;
    struct sockaddr_in sockadd;
    KnownNode* known_node_to_check;

    // Configuro un sockadd_in para intentar conectarnos
    // todos los nodos tienen la misma familia
    sockadd_size = sizeof(sockadd);
    sockadd.sin_family = AF_INET;

    //code = REQUEST_LIST;
    printf("Escaneando nodos conocidos...\n");
    for(i=0; i < server.known_nodes_length; i++) {
        sd_known_node = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);

        // Tomo el KnownNode
        known_node_to_check = server.known_nodes[i];
        // Seteo la ip del KnownNode en el sockadd_in
        sockadd.sin_addr.s_addr = inet_addr(known_node_to_check->ip);
        // Seteo el port del KnownNode en el sockadd_in
        sockadd.sin_port = htons(known_node_to_check->port);

        //Intentamos conectarnos para ver si esta vivo!
        if (connect(sd_known_node, (struct sockaddr*)&sockadd, sockadd_size) >= 0) {
            printf("IP=%s port=%u (nodo ACTIVO), Intercambiando lista de archivos\n", known_node_to_check->ip, known_node_to_check->port);
            // marco el KnownNode como ACTIVO, envio y desconecto
            known_node_to_check->status = KNOWN_NODE_ACTIVE;
            //send_message(sd_node, code, "hola;chau;");
            close(sd_known_node);
        } else {
            printf("IP=%s port=%u (nodo INACTIVO)\n", known_node_to_check->ip, known_node_to_check->port);
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
        } else {
            task_parse_list_message(pipe_buf);
            // Emitir una SIGNAL (pthread_cond_broadcast(mutex, cond))
            // para despertar los workers
        }
    }
    printf("%s\n", "HIJO Terminando...");
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
