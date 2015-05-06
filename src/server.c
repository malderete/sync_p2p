#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "include/crc.h"
#include "include/file_system.h"
#include "include/server.h"
#include "include/ipc_protocol.h"
#include "include/protocol.h"
#include "include/sessions.h"


extern Server server;
extern SessionList* sessions;
extern int pipe_fds[2];


/*
 * Esta funcion busca un KnownNode
 * cuya ip coincide con la ip dada.
 * Retorna el puerto de su server.
 */
int server_get_port_for_active_node(char *ip) {
    int i, port;
    KnownNode *known_node;

    port = -1;
    for(i=0; i < server.known_nodes_length; i++) {
        known_node = server.known_nodes[i];
        if ((known_node->status == KNOWN_NODE_ACTIVE) && (strncmp(known_node->ip, ip, strlen(known_node->ip)) == 0)) {
            port = known_node->port;
            break;
        }
    }
    return port;
}


/*
 * Esta funcion marca el KnownNode
 * cuya ip coincide con la dada como activo.
 * Retorna 1 si fue posible, 0 en otro caso.
 */
int server_set_node_as_active(char *ip) {
    KnownNode *known_node;
    int i, done;

    done = 0;
    for(i=0; i < server.known_nodes_length; i++) {
        known_node = server.known_nodes[i];
        if (strncmp(known_node->ip, ip, strlen(known_node->ip)) == 0) {
            // Este bloque solo es semantico para mostar una posible situacion
            switch (known_node->status) {
                case KNOWN_NODE_INACTIVE:
                    // Nodo inicia nunca fue visto
                    server.known_nodes[i]->status = KNOWN_NODE_ACTIVE;
                    done = 1;
                    break;
                case KNOWN_NODE_ACTIVE:
                    // Nodo inicia pero ya esta como conocido
                    server.known_nodes[i]->status = KNOWN_NODE_ACTIVE;
                    done = 1;
                    break;
            }
        }
    }
    return done;
}


/*
 * Esta funcion envia el CRC del archivo
 * filename al cliente especificado en la session.
 * Retorna la cantidad de bytes enviados.
 */
int server_send_file_info(Session *session, char *filename) {
    FileInfo *file_info;
    char *crc_sum;
    int nbytes;

    nbytes = 0;
    file_info = NULL;
    crc_sum = NULL;
    file_info = file_system_get(filename);
    if (file_info) {
        nbytes = crc_md5sum_wrapper(file_info->abs_path, &crc_sum);
        //printf("Server CRC: %s (%d bytes)\n", crc_sum, nbytes);
        nbytes = protocol_send_message(session->fd, RESPONSE_CRC, crc_sum, nbytes);
    } else {
        // archivo no encontrado
        nbytes = protocol_send_message(session->fd, NOT_FOUND, filename, strlen(filename));
    }
    return nbytes;
}


/*
 * Esta funcion envia un segmento del archivo
 * filename (si este existe) al cliente especificado
 * en la session.
 * Retorna la cantidad de bytes enviados.
 */
int server_send_file_segment(Session *session, char *filename) {
    FileInfo *file_info;
    char send_buffer[PAYLOAD_SIZE];
    int nbytes, data_sent_size, protocol_code, message_size;

    nbytes = 0;
    file_info = NULL;
    file_info = file_system_get(filename);
    if (file_info) {
        if ((file_info->bytes - session->offset) >= PAYLOAD_SIZE) {
            message_size = PAYLOAD_SIZE;
        } else {
            message_size = (file_info->bytes - session->offset);
        }
        if ((session->offset + message_size) >= file_info->bytes) {
            // Si es el ultimo segmente cambiamos el codigo
            // asi avisamos al cliente que debe hacer close()
            protocol_code = LAST_FILE_SEGMENT;
        } else {
            protocol_code = FILE_SEGMENT;
        }

        memcpy(send_buffer, file_info->content + session->offset, message_size);
        nbytes = protocol_send_message(session->fd, protocol_code, send_buffer, message_size);
        data_sent_size = nbytes;
        // Actualizamos el offset
        session->offset += data_sent_size;
    } else {
        // archivo no encontrado
        nbytes = protocol_send_message(session->fd, NOT_FOUND, filename, strlen(filename));
    }
    return nbytes;
}


/*
 * Esta funcion es la encargada de manejar los codigos/mensajes
 * del protocolo, aca se decide que hacer :)
 * Funciona como un dispatcher, es decir, toma una desicion en base
 * al codigo (identificador del mensaje) en nuestro caso
 */
void handle_message(int sd, uint16_t message_code, char *message) {
    Session *ses;
    char *ipc_buffer;

    ses = get_session(sessions, sd);
    printf("[*] Cliente IP: %s, Codigo: %u, Mensaje: %s\n", ses->ip, message_code, message);
    // Aca esta el dispatcher
    // Se deberia tener una entrada del switch por cada codigo
    switch (message_code) {
        case REQUEST_LIST:
            // Nos aseguramos que el nodo este activo para nosotros
            server_set_node_as_active(ses->ip);
            printf("[*] El cliente %d envia la lista de archivos\n", sd);
            ipc_buffer = (char *)malloc(strlen(ses->ip) + strlen(message) + sizeof(char));
            sprintf(ipc_buffer, "%s@%s", ses->ip, message);

            ipc_send_message(pipe_fds[1], ipc_buffer);
            free(ipc_buffer);
            break;
        case REQUEST_CRC:
            printf("[*] El cliente %d solicita el CRC de un archivo\n", sd);
            server_send_file_info(ses, message);
            break;
        case FILE_SEGMENT:
            printf("[*] El cliente %d solicita un segmento de un archivo\n", sd);
            server_send_file_segment(ses, message);
            break;
        case BYE:
            printf("[*] El cliente %d se desea desconectar\n", sd);
            break;
    }
}


/*
 * Inicia el bucle principal del proceso PADRE
 * que es el servidor de archivos
 */
int server_init_stack(void) {
    // Conjuntos para monitorear con select()
    fd_set master;
    fd_set read_fds;
    int fdmax;

    // sd del server
    int listener;
    // sd de cada cliente aceptado
    int newfd;
    // direccion de cada client
    struct sockaddr_storage client;
    socklen_t addrlen;

    int nbytes;
    char remoteIP[INET6_ADDRSTRLEN];

    //setsockopt() SO_REUSEADDR
    int yes = 1;
    int who, rv;

    struct addrinfo hints, *ai, *p;

    // Necesarios para el protocolo
    uint16_t message_code;
    char message_buffer[MAX_SIZE];

    // Iniciamos los conjuntos en vacios
    FD_ZERO(&master);
    FD_ZERO(&read_fds);

    // Session
    Session *s;

    // Tomamos un socket para hacer bind() y listen()
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET; // IPv4
    hints.ai_socktype = SOCK_STREAM; // un socket de STREAM
    hints.ai_flags = AI_PASSIVE; // Queremos poder hacer bind()

    // Hacemos la consulta
    if ((rv = getaddrinfo(NULL, server.server_port, &hints, &ai)) != 0) {
        fprintf(stderr, "Server: %s\n", gai_strerror(rv));
        exit(1);
    }

    for(p=ai; p != NULL; p=p->ai_next) {
        listener = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (listener < 0) {
            continue;
        }
        // Reutilizamos para evitar el "address already in use"
        setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
        if (bind(listener, p->ai_addr, p->ai_addrlen) < 0) {
            close(listener);
            continue;
        }
        // Pudimos hacer bind() entonces salgamos del loop!!!
        break;
    }
    // No deberiamos estar aca (p == NULL) =(
    if (p == NULL) {
        fprintf(stderr, "Server: Error en bind\n");
        exit(2);
    }
    // No la usamos mas, liberamos memoria
    freeaddrinfo(ai);
    if (listen(listener, BACKLOG) == -1) {
        perror("listen");
        exit(3);
    }
    server.status = SERVER_STATUS_ACTIVE;
    printf("Server listening on PORT %s\n", server.server_port);

    // Agregamos el server al conjunto 'maestro'
    FD_SET(listener, &master);
    // el maximo FD es listener...(por ahora)
    fdmax = listener;

    while(server.status == SERVER_STATUS_ACTIVE) {
        // copiamos 'master' y esperamos que algo suceda :P
        read_fds = master;
        // Si queremos timeout debemos reemplazar el ultimo NULL
        // por un struct timeval
        if (select(fdmax+1, &read_fds, NULL, NULL, NULL) == -1) {
            perror("select");
            exit(4);
        }

        // Recorremos las conexiones y buscamos los datos para leer
        for(who=0; who <= fdmax; who++) {
            // Algo que leer =D!
            if (FD_ISSET(who, &read_fds)) {
                // Un FD es el server tenemos nuevas conexiones
                if (who == listener) {
                    addrlen = sizeof(client);
                    newfd = accept(listener,
                            (struct sockaddr *)&client,
                            &addrlen);

                    if (newfd == -1) {
                        perror("accept");
                    } else {
                        // Agregamos el newfd al conjunto 'master'
                        // Si es necesario actualizamod fdmax ;)
                        FD_SET(newfd, &master);
                        if (newfd > fdmax) {
                            fdmax = newfd;
                        }
                        printf("Server: Nueva conexion desde %s en "
                            "socket %d\n",
                            inet_ntop(client.ss_family,
                                &(((struct sockaddr_in*)&client)->sin_addr),
                                remoteIP, INET_ADDRSTRLEN),
                                newfd);
                        s = create_session();
                        s->fd = newfd;
                        s->ip = remoteIP;
                        sessions = add_session(sessions, s);
                    }
                } else { // Algun cliente tiene datos...
                     memset(message_buffer, 0, MAX_SIZE);
                     nbytes = protocol_read_message(who, &message_code, message_buffer);
                     // Verificamos posibles errores
                     if (nbytes <= 0) {
                       // Error o un cliente que hizo close()
                        if (nbytes == 0) {
                            printf("Server: socket %d se desconecto\n", who);
                            // Eliminamos la session
                            sessions = remove_session(sessions, who);
                        } else {
                            printf("Problemas no esperados =(...!");
                            perror("recv");
                        }
                        // cerramos el fd y lo eliminamos del conjunto 'master'
                        close(who);
                        FD_CLR(who, &master);
                     } else { // Tenemos un mensaje del cliente
                        // Llamamos al manejador de mensajes xD
                        handle_message(who, message_code, message_buffer);
                    }
                }
            }
        }
    }
    printf("[*] Finalizando servidor: %s\n", server.name);
    // No permitimos ni enviar ni recibir!
    shutdown(listener, SHUT_RDWR);
    return 0;
}
