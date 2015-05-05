#ifndef IPC_PROTOCOL_H_
#define IPC_PROTOCOL_H_


/*            Protocolo IPC
*  ******************************
*  * size  *     DATA           *
*  *       *                    *
*  ******************************
*
*/

// Protocolo Header
#define IPC_HEADER_SIZE_LENGTH 2 // porque usamos uint16_t


int ipc_read_message(int sd, char **message);
int ipc_send_message(int sd, char *ip, char *message);
#endif /* IPC_PROTOCOL_H_ */
