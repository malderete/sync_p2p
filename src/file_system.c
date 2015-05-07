#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <dirent.h> //gnu opendir
#include <fcntl.h>
#include <unistd.h> 
#include <sys/stat.h>
#include <string.h>
#include <sys/mman.h>

#include "include/file_system.h"

static FileInfo **files_table;
static int size;


/*
 *  Lista *UNICAMENTE* archivos en u directorio,
 *  excluye archivos ocultos y carpetar.

 */
void _list_dir(const char *dir_name, int *size) {
    DIR *dp;
    struct dirent *ep;
    struct stat file_stat;
    int i;
    int fd;
    FileInfo *tmp;

    const int len_dir_name = strlen(dir_name);
    i = 0;
    dp = opendir(dir_name);
    if (dp != NULL) {
        while((ep = readdir(dp))) {
            if (ep->d_name[0] != '.') {
                i++;
            }
        }
        closedir(dp);
    } else {
        perror("No fue posible abrir el directorio");
    }

    dp = opendir(dir_name);
    files_table = (FileInfo **) calloc(i, sizeof(FileInfo *));
    i = 0;
    while((ep = readdir(dp))) {
        if (ep->d_name[0] != '.') {
            char *path = malloc(len_dir_name + strlen(ep->d_name) + 2);
            memset(path, 0, len_dir_name + strlen(ep->d_name) + 2);
            //Concatenation
            strncpy(path, dir_name, len_dir_name);
            strncat(path, ep->d_name, strlen(ep->d_name));
            //end concatenation
            stat(path, &file_stat);
            if (!S_ISREG(file_stat.st_mode)) {
                printf("[!] Skipping folder: %s\n", path);
                continue;
            }

            //alocamos un FileInfo
            tmp = (FileInfo *)malloc(sizeof(FileInfo));
            tmp->filename = ep->d_name;
            tmp->abs_path = path;
            tmp->bytes = file_stat.st_size;
            //Archivo mapeado en memoria
            fd = open(path, O_RDONLY);
            tmp->content = mmap(0, file_stat.st_size, PROT_READ, MAP_SHARED, fd, 0);
            close(fd);
            //Agregamos al array
            files_table[i] = tmp;
            i++;
        }
    }
    closedir(dp);
    *size = i;
}


/*
 * Serializamos la info acerca de los archivos
 * en un buffer
 */
void serialize_files(char **buffer) {
    int j = 0;
    int paddin = 0;
    int size_local = 0;
    int alloc = 0;

    for(j=0; j < size; j++) {
        alloc += strlen(files_table[j]->filename) + (sizeof(char) * 2) + sizeof(files_table[j]->bytes);
    }
    *buffer = (char *)malloc(alloc + 1);

    for(j=0; j < size; j++) {
        size_local = sprintf(*buffer + paddin, "%s:%d;", files_table[j]->filename, files_table[j]->bytes);
        paddin += size_local;
    }
}


FileInfo *file_system_get(char *filename) {
    int i;
    FileInfo *aux;

    aux = NULL;
    i = 0;
    while(i < size) {
        aux = files_table[i];
        if (strncmp(aux->filename, filename, strlen(aux->filename)) == 0) {
            break;
        }
        i++;
    }
    return aux;
}


void _filesystem_print(int size) {
    int j = 0;
    for(j=0; j < size; j++) {
        printf("%s:%u\n", files_table[j]->filename, (unsigned)files_table[j]->bytes);
    }
}


/*
 * Inicializa el modulo de File System
 */
void filesystem_load(const char *dir_name) {
    _list_dir(dir_name, &size);
    fprintf(stderr,"-=[Archivos a compartir]=-\n");
    _filesystem_print(size);
    fprintf(stderr,"-=========================-\n");
}


/*
int
main (int argc, char* argv[]) {
    if (argc < 2) {
        fprintf (stderr, "usage: %s <file index>\n", argv[0]);
        return 1;    
    }

    off_t i;
	char *buffer;
	buffer = malloc(500);
	const char* dir_name = "/home/tincho/proyectos/iw/";
	list_dir(dir_name);
	//printf("Size: %d\n", size);
	//printf("Listando el directorio: %s\n", dir_name);
	//print(size);
	printf("Serializando la lista...\n");
	serialize_files(buffer);
	printf("BUFFER: %s\n", buffer);
	printf("SIZE: %d\n", strlen(buffer));
    FileInfo *f_aux;
    
    f_aux = (FileInfo*)malloc(sizeof(FileInfo));
    f_aux = files_table[atoi(argv[1])];
    printf("Filename: %s,  Size: %d bytes\n", f_aux->filename, f_aux->bytes);
    i = 0;
    fprintf(stderr, "Contenido\n\n");
    for(i=0; i < f_aux->bytes; i++) {
        printf("%c", f_aux->content[i]);
    }
    fprintf(stderr, "\n\nFin\n");
	return 0;
}

*/
