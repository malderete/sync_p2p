#ifndef SERVER_H_
#define SERVER_H_

#include <netinet/in.h> //uint16_t

// Extras
#define PORT "8888"   // Donde queremos escuchar
#define BACKLOG 10 // Cuantas conexiones permitimos en la cola de pendientes

void handle_message(int sd, uint16_t message_code, char * message);
int server_init_stack(void);


#endif /* SERVER_H_ */
