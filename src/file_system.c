#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <dirent.h> //gnu opendir
#include <sys/stat.h>
#include <string.h>

#include "include/file_system.h"

static FileInfo **files_table;
static int size;


/**
 *  List files in a directory, exclude hidden files
 **/
void
_list_dir(const char *dir_name, int *size) {
	DIR *dp;
	struct dirent *ep;
	struct stat file_status;
	int i;
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
			//Allocate a new node_s and add it to the array
			tmp = (FileInfo *)malloc(sizeof(FileInfo));
			tmp->filename = ep->d_name;
			char *path = malloc(len_dir_name + strlen(ep->d_name) + 2);
			//Concatenation
			strncpy(path, dir_name, len_dir_name);
			strncat(path, ep->d_name, strlen(ep->d_name));
			//end concatenation
			stat(path, &file_status);
			tmp->bytes = file_status.st_size;
			files_table[i] = tmp;
			i++;
		}
	}
	closedir(dp);
	*size = i;
}


void
serialize_files(char *buffer) {
	/*
	 * hola:21;asdad:123\n
	 */
	int j = 0;
	int paddin = 0;
	int size_local = 0;
	for(j=0; j < size; j++) {
		size_local = snprintf(buffer + paddin, 1024, "%s:%d;", files_table[j]->filename, files_table[j]->bytes);
		paddin += size_local;
		//printf("pad: %i\n", paddin);
	}
	//Set the end of string
	buffer[paddin] = '\0';
	//printf("BUFFER: %s\n", buffer);
	//printf("BUFFER: %d\n", strlen(buffer));
}


void
print(int size) {
	int j = 0;
	for(j=0; j < size; j++) {
		printf("%s:%u\n", files_table[j]->filename, (unsigned)files_table[j]->bytes);
	}
}

void list_dir(const char *dir_name) {
	_list_dir(dir_name, &size);
}

/*
int
main (void) {
	char *buffer;
	buffer = malloc(500);
	const char* dir_name = "/home/tincho/proyectos/iw/";
	list_dir(dir_name, &size);
	//printf("Size: %d\n", size);
	//printf("Listando el directorio: %s\n", dir_name);
	//print(size);
	printf("Serializando la lista...\n");
	serialize_files(buffer);
	printf("BUFFER: %s\n", buffer);
	printf("SIZE: %d\n", strlen(buffer));
	return 0;
}
*/
