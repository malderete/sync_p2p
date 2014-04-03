#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "include/main.h"
#include "include/server.h"
#include "include/protocol.h"
#include "include/sessions.h"


extern Server server;
extern SessionList* sessions;
extern int pipe_fds[2];
//extern Config config;



/*
* Esta funcion es la encargada de manejar los codigos/mensajes
* del protocolo, aca se decide que hacer :)
* Funciona como un dispatcher, es decir, toma una desicion en base
* al codigo (identificador del mensaje) en nuestro caso
*/
void handle_message(int sd, uint16_t message_code, char * message) {
    Session * ses;
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

            sprintf(big_buffer, "%s@%s", ses->ip,  message);
            write(pipe_fds[1], big_buffer, strlen(big_buffer));
            free(big_buffer);
            printf("Longitud insertada en pipe: %lu\n", strlen(big_buffer));
            break;
        case REQUEST_FILE:
            printf("El cliente %d solicita descargar/info sobre un archivo\n", sd);
            break;
        case FILE_SEGMENT:
            printf("El cliente %d solicita un segmento de un archivo\n", sd);
            break;
        case BYE:
            printf("El cliente %d se desea desconectar\n", sd);
            write(pipe_fds[1], "FIN", 3);
            break;
        case EXIST_FILE: //Este es un mensaje que no se si tendremos...
                        // La info la podemos mandar en REQUEST_FILE
            printf("El cliente %d solicita info de un archivo\n", sd);
            break;
    }
    // Por ahora es un 'triste' ACK de prueba...xD
    if (send(sd, "Gracias", 7, 0) == -1) {
        perror("send");
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
    int i, rv;

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
        for(i = 0; i <= fdmax; i++) {
            // Algo que leer =D!
            if (FD_ISSET(i, &read_fds)) {
                // Un FD es el server tenemos nuevas conexiones
                if (i == listener) {
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
                     nbytes = read_message(i, &message_code, message_buffer);
                     // Verificamos posibles errores
                     if (nbytes <= 0) {
                       // Error o un cliente que hizo close()
                        if (nbytes == 0) {
                            printf("Server: socket %d se desconecto\n", i);
                            // Eliminamos la session
                            sessions = remove_session(sessions, i);
                        } else {
                            printf("Problemas no esperados =(...!");
                            perror("recv");
                        }
                        // cerramos el fd y lo eliminamos del conjunto 'master'
                        close(i);
                        FD_CLR(i, &master);
                     } else { // Tenemos un mensaje del cliente
                        // Llamamos al manejador de mensajes xD
                        handle_message(i, message_code, message_buffer);
                    }
                }
            }
        }
    }
    return 0;
}
