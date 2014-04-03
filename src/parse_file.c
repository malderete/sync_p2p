#include <stdio.h>
#include <stdlib.h>

#include "include/parse_file.h"


/*
 *  Lee un archivo linea a linea para
 *  carar la lista de nodos conocidos.
 */
void parse_nodes_file(char* filename) {
    FILE *infile;
    char line_buffer[BUFSIZ]; // Definido en stdio.h
    char line_number;

    infile = fopen(filename, "r");
    if (!infile) {
        printf("No se pudo abrir el archivo %s\n", filename);
    }
    printf("Archivo abierto %s\n\n", filename);

    line_number = 0;
    while (fgets(line_buffer, sizeof(line_buffer), infile)) {
        ++line_number;
        printf("%s", line_buffer);
    }
}
