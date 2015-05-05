#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "include/main.h"
#include "include/file_system.h"
#include "include/server.h"
#include "include/protocol.h"
#include "include/sessions.h"


extern Server server;
extern SessionList* sessions;
extern int pipe_fds[2];


int server_send_file_info(Session *session, char *filename) {
    FileInfo *file_info;
    char send_buffer[PAYLOAD_SIZE];
    int nbytes;

    nbytes = 0;
    file_info = NULL;
    file_info = file_system_get(filename);
    if (file_info) {
        memcpy(send_buffer, &file_info->bytes, sizeof(file_info->bytes));
        nbytes = protocol_send_message(session->fd, REQUEST_FILE, send_buffer, strlen(send_buffer));
    }
    return nbytes;
}


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
        // Bytes enviados sin el header
        data_sent_size = nbytes - (HEADER_CODE_LENGTH + HEADER_SIZE_LENGTH);
        // Actualizamos el offset
        session->offset += data_sent_size;
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

    ses = get_session(sessions, sd);

    printf("Cliente IP: %s, Codigo: %u, Mensaje: %s\n", ses->ip, message_code, message);
    // Aca esta el dispatcher
    // Se deberia tener una entrada del switch por cada codigo
    switch (message_code) {
        case REQUEST_LIST:
            printf("El cliente %d solicita la lista de archivos\n", sd);
            char * big_buffer = malloc(PIPE_SIZE); // Esto hay que moverlo de aca, alocamos al pedo!!!
            // Escribimos en el pipe el IP y la lista de archivos del cliente
            // separados por @. Ejemplo: 192.168.1.102@file1;file2;file3;...;fileN;

            //Mejor implementacion
            // char * big_buffer = malloc(sizeof(unsigned int) + sizeof(ses->ip) + 1 + sizeof(message));
            // ejemplo de mensaje
            //   40192.168.1.12@text.txt,example.py,dns.c
            // ip = '192.168.1.12'
            // msg = 'text.txt,example.py,dns.c'
            // size = 2 + 12 + 1 + 25 = 40 este valor le dice al otro lado del pipe cuando alocar ;)
            //sprintf(big_buffer, "%u%s@%s", size, ses->ip,  message);

            sprintf(big_buffer, "%s@%s", ses->ip,  message);
            write(pipe_fds[1], big_buffer, strlen(big_buffer));
            printf("Longitud insertada en pipe: %lu\n", strlen(big_buffer));
            free(big_buffer);
            break;
        case REQUEST_FILE:
            printf("El cliente %d solicita descargar/info sobre un archivo\n", sd);
            server_send_file_info(ses, message);
            break;
        case FILE_SEGMENT:
            printf("El cliente %d solicita un segmento de un archivo\n", sd);
            server_send_file_segment(ses, message);
            break;
        case BYE:
            printf("El cliente %d se desea desconectar\n", sd);
            write(pipe_fds[1], "FIN", 3);
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

    int listener;     // Server fd
    int newfd;        // fd de cada cliente aceptado
    struct sockaddr_storage client; // client address
    socklen_t addrlen;

    //char buf[MAX_SIZE];    // buffer for client data
    int nbytes;

    char remoteIP[INET6_ADDRSTRLEN];

    int yes = 1;        // for setsockopt() SO_REUSEADDR, below
    int who, rv;

    struct addrinfo hints, *ai, *p;

    // Necesarios para el protocolo
    uint16_t message_code;
    char message_buffer[MAX_SIZE];

    // Iniciamos los conjuntos en vacios...
    FD_ZERO(&master);
    FD_ZERO(&read_fds);

    // Session
    Session* s;

    // Tomamos un socket para hacer bind() y listen()...
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC; // No importa la familiar IPv4 o IPv6
    hints.ai_socktype = SOCK_STREAM; // un socket de STREAM
    hints.ai_flags = AI_PASSIVE; // Queremos poder hacer bind()
    // Hacemos la consulta
    if ((rv = getaddrinfo(NULL, PORT, &hints, &ai)) != 0) {
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
    printf("Server listening on PORT %s\n", PORT);

    // Agregamos el server al conjunto 'maestro'
    FD_SET(listener, &master);
    // el maximo FD es listener...(por ahora)
    fdmax = listener;

    for(;;) {
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
                            printf("maximo actual: %d\n", fdmax);
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
    return 0;
}
