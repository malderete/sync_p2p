#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "include/crc.h"


int crc_md5sum_wrapper(char *absolute_path, char **md5sum) {
    FILE *file = NULL;
    char *final_command;
    char aux[1024];

    final_command = (char *)malloc(strlen(MD5_BIN_PATH) + strlen(absolute_path) + 2);
    sprintf(final_command, "%s %s", MD5_BIN_PATH, absolute_path);

    //file = popen ("/usr/bin/md5sum /home/tincho/config.json", "r");
    file = popen(final_command, "r");
    if (file == NULL) {
        perror ("No se puede abrir /usr/bin/md5sum");
        exit (-1);
    }

    //Se lee la primera linea y se hace un bucle, hasta fin de fichero,
    //para ir sacando por pantalla los resultados.
    fgets(aux, 1000, file);
    const char *s = " ";
    // asignamos el md5 y limpiamos
    *md5sum = strtok(aux, s);
    pclose(file);
    free(final_command);

    return strlen(*md5sum);
}
