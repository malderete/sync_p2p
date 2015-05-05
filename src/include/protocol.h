#ifndef PROTOCOL_H_
#define PROTOCOL_H_


/*              Protocolo
*  *******************************************
*  * code * size  *     DATA                 *
*  *      *       *                          *
*  *******************************************
*
*/

// tamanio maximo
#define MAX_SIZE 65535
//#define MAX_SIZE 1 << 32
// Protocolo Header
#define HEADER_CODE_LENGTH 2 // porque usamos uint16_t con uint32_t cambia a 4
#define HEADER_SIZE_LENGTH 2
// Codigos
#define REQUEST_LIST 100
#define REQUEST_CRC 200
#define FILE_SEGMENT 300
#define LAST_FILE_SEGMENT 310
#define BYE 400
#define EXIST_FILE 600
// tamaño usado para enviar archivos
#define PAYLOAD_SIZE 8192


int read_n_bytes(int sd, void *buffer, int n);
int protocol_read_message(int sd, uint16_t * code, char *message);
int protocol_send_message(int sd, uint16_t code, char *message, int message_size);



#endif /* PROTOCOL_H_ */
