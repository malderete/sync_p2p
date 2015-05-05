#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>


#include "include/main.h"
#include "include/server.h"
#include "include/protocol.h"
#include "include/tasks.h"


extern Server server;
extern int pipe_fds[2];
extern char pipe_buf[PIPE_SIZE];
static int finish_downloading;

// Mutexes and conditions
pthread_mutex_t task_mutex;
pthread_cond_t can_consume;
pthread_cond_t can_produce;

/*
 * Realiza la descarga y guarda el archivo en el FS
 */
void downloader_download(Task *task) {
    printf("Started download: filename: %s (%u bytes) from %s\n", task->filename, task->total, task->ip);
    sleep(45);
    printf("Finished download: filename: %s\n", task->filename);
}

/*
* Esta funcion representa un thread WORKER
* encargado de descargar un archivo de un nodo
* debe tomar los trabajos de una cola
*/
void *downloader_worker_thread(void *worker_data) {
    long my_id;
    Task *task;

    my_id = (long)worker_data;

    pthread_mutex_lock(&task_mutex);
    while (!finish_downloading) {
        while (task_size() < 1) {
            if (finish_downloading) {
                break;
            }
            printf("Waiting condition can_consume: thread %ld\n", my_id);
            pthread_cond_wait(&can_consume, &task_mutex);
        }
        if (finish_downloading) {
            break;
        }
        task = task_get();
        pthread_cond_signal(&can_produce);
        pthread_mutex_unlock(&task_mutex);
        downloader_download(task);
        pthread_mutex_lock(&task_mutex);  //(?)
    }
    pthread_mutex_unlock(&task_mutex); //(?)
    printf("Exiting downloader_worker_thread: %ld\n", my_id);
    pthread_exit(NULL);
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
    KnownNode *known_node_to_check;

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
    long t0, t1;
    pthread_t threads[2];
    pthread_attr_t attr;

    // Iniciamos las tareas
    tasks_initialize();
    // Buscamos nodos activos
    client_broadcast_nodes();
    // Iniciamos el mutex y las condiciones
    pthread_mutex_init(&task_mutex, NULL);
    pthread_cond_init (&can_consume, NULL);
    pthread_cond_init (&can_produce, NULL);
    // Iniciar los workers threads
    t0 = 0;
    t1 = 1;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
    pthread_create(&threads[0], &attr, downloader_worker_thread, (void *)t0);
    pthread_create(&threads[1], &attr, downloader_worker_thread, (void *)t1);

    done = 0;
    finish_downloading = 0;
    printf("Downloader Scheduler esperando datos...\n");
    while(!done) {
        read(pipe_fds[0], pipe_buf, PIPE_SIZE);
        fprintf(stderr, "HIJO Leido %s (%lu bytes)\n", pipe_buf, strlen(pipe_buf));
        if (strncmp(pipe_buf, STOP_MESSAGE, 3) == 0) {
            printf("Leido FIN\n");
            done = 1;
            finish_downloading = 1;
            pthread_cond_broadcast(&can_consume);
        } else {
            task_parse_list_message(pipe_buf);
            pthread_cond_broadcast(&can_consume);
        }
    }
    // Esperamos que los threads terminen
    pthread_join(threads[0], NULL);
    pthread_join(threads[1], NULL);

    // limpieza
    pthread_attr_destroy(&attr);
    pthread_mutex_destroy(&task_mutex);
    pthread_cond_destroy(&can_consume);
    pthread_cond_destroy(&can_produce);
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
    printf("%s\n", "HIJO Terminado");
    exit(0);
}
