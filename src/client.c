#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>


#include "include/crc.h"
#include "include/file_system.h"
#include "include/server.h"
#include "include/ipc_protocol.h"
#include "include/protocol.h"
#include "include/tasks.h"


extern Server server;
extern int pipe_fds[2];
static int finish_downloading;

// Mutexes and conditions
pthread_mutex_t task_mutex;
pthread_cond_t can_consume;
pthread_cond_t can_produce;


/*
 * Realiza la descarga y guarda el archivo en el FS
 */
void downloader_download(Task *task) {
    struct sockaddr_in sockadd;
    int sd, port, nbytes;
    uint16_t code;
    char payload[PAYLOAD_SIZE];
    char *file_path;
    char *local_crc_sum;
    FILE *file;

    code = 0 ;
    port = server_get_port_for_active_node(task->ip);
    sockadd.sin_family = AF_INET;
    sockadd.sin_addr.s_addr = inet_addr(task->ip);
    sockadd.sin_port = htons(port);
    sd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    connect(sd, (struct sockaddr*)&sockadd, sizeof(sockadd));


    file_path = (char *)malloc(strlen(server.download_directory) + strlen(task->filename) + sizeof(char)*2);
    sprintf(file_path, "%s%s", server.download_directory, task->filename);

    file = fopen(file_path, "w");

    printf("[*] Comenzando descarga %s (%s) de %s:%d\n", task->filename, file_path, task->ip, port);
    while (code != LAST_FILE_SEGMENT) {
        protocol_send_message(sd, FILE_SEGMENT, task->filename, strlen(task->filename));
        nbytes = protocol_read_message(sd, &code, payload);
        fwrite(payload, sizeof(char), nbytes, file);
    }
    fclose(file);
    // calculamos el CRC local
    crc_md5sum_wrapper(file_path, &local_crc_sum);
    // pedimos el CRC al server
    protocol_send_message(sd, REQUEST_CRC, task->filename, strlen(task->filename));
    nbytes = protocol_read_message(sd, &code, payload);


    if (strncmp(payload, local_crc_sum, nbytes) == 0) {
        printf("[*] Descarga valida %s el CRC %s es correcto\n", file_path, local_crc_sum);
    } else {
        //TODO: Una mejora seria retransmitir el archivo!
        printf("[!] Descarga invalida eliminando %s el CRC %s no concuerda con %s\n",
                file_path, local_crc_sum, payload
        );
        if (unlink(file_path) != 0) {
            printf("[!] Error al eliminar archivo invalido: %s\n", file_path);
        }
    }
    // Desconectarnos del server ya terminamos este archivo!
    close(sd);
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
            printf("[*] Esperando condicion can_consume: thread %ld\n", my_id);
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
    printf("[*] Saliendo de downloader_worker_thread: %ld\n", my_id);
    pthread_exit(NULL);
}


/*
 * Intenta conectarse a cada uno de los nodos conocidos
 * para intercambiar la lista de archivos y marcar el nodo
 * como activo.
 * Intenta realizar una conexion TCP con cada nodo
 * (connect() y close())
 */
void client_broadcast_nodes() {
    int sd_known_node, sockadd_size, i;
    uint16_t code;
    struct sockaddr_in sockadd;
    KnownNode *known_node_to_check;
    char *files_list_buffer;

    // Configuro un sockadd_in para intentar conectarnos
    // todos los nodos tienen la misma familia
    sockadd_size = sizeof(sockadd);
    sockadd.sin_family = AF_INET;

    code = REQUEST_LIST;
    files_list_buffer = (char *)malloc(1024);
    serialize_files(files_list_buffer);
    printf("[*] Buscando nodos conocidos...\n");
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
            printf("[*] IP=%s Port=%u (nodo ACTIVO), Intercambiando lista de archivos\n", known_node_to_check->ip, known_node_to_check->port);
            // marco el KnownNode como ACTIVO, envio y desconecto
            known_node_to_check->status = KNOWN_NODE_ACTIVE;
            protocol_send_message(sd_known_node, code, files_list_buffer, strlen(files_list_buffer));
            close(sd_known_node);
        } else {
            printf("[*] IP=%s port=%u (nodo INACTIVO)\n", known_node_to_check->ip, known_node_to_check->port);
        }
    }
}


/*
 * Inicia el bucle principal del proceso HIJO
 * que planifica las descargas
 */
void downloader_init_stack(void) {
    char *ipc_buffer;
    char *ipc_buffer_cpy;
    char *node_ip;
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
    pthread_cond_init(&can_consume, NULL);
    pthread_cond_init(&can_produce, NULL);
    // Iniciar los workers threads
    t0 = 0;
    t1 = 1;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
    pthread_create(&threads[0], &attr, downloader_worker_thread, (void *)t0);
    pthread_create(&threads[1], &attr, downloader_worker_thread, (void *)t1);

    done = 0;
    finish_downloading = 0;
    printf("[*] Downloader Scheduler esperando datos\n");
    while(!done) {
        ipc_buffer = NULL;
        ipc_read_message(pipe_fds[0], &ipc_buffer);
        fprintf(stderr, "[*] HIJO Leido %s (%lu bytes)\n", ipc_buffer, strlen(ipc_buffer));
        if (strncmp(ipc_buffer, IPC_STOP_MESSAGE, strlen(IPC_STOP_MESSAGE)) == 0) {
            printf("[*] IPC_STOP_MESSAGE recibido\n");
            done = 1;
            finish_downloading = 1;
            pthread_cond_broadcast(&can_consume);
        } else {
            ipc_buffer_cpy = NULL;
            ipc_buffer_cpy = strdup(ipc_buffer);
            node_ip = strtok(ipc_buffer_cpy, "@");
            if (server_set_node_as_active(node_ip) > 0) {
                task_parse_list_message(ipc_buffer);
                pthread_cond_broadcast(&can_consume);
            } else {
                printf("[!] Ignorando mensaje IP desconocida/invalida: %s\n", node_ip);
            }
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

    //Task* t = task_get();
    //while(t != NULL) {
        //printf("IP: %s\n", t->ip);
        //printf("Filename: %s\n", t->filename);
        //printf("Total: %d\n\n", t->total);
        //t = task_get();
    //}
    printf("[*] %s\n", "Finalizando Planificador de descargas");
    exit(0);
}
