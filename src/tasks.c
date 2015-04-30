#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <pthread.h>

#include "include/tasks.h"


static Task** tasks;
//Current index
static size_t current_tasks = 0;
static size_t max_tasks = 100;
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;



int
tasks_initialize() {
	int i;
    //Set the limit of tasks allowed!
    tasks = calloc(max_tasks, sizeof(Task *));

    if(tasks == NULL) {
		perror("taks_initialize():");
		return -1;
    }
    for(i=0; i < max_tasks; i++) {
    	tasks[i] = NULL;
    }

    return 0;
}

int
task_add(Task* task) {
	int i;
	fprintf(stderr, "task_add()\n");
	pthread_mutex_lock(&mutex);
	for(i=0; i < max_tasks; i++) {
		if (tasks[i] == NULL) {
			tasks[i] = task;
			current_tasks++;
			pthread_mutex_unlock(&mutex);
			break;
		}
	}
	fprintf(stderr, "Saliendo: task_add()\n");
    return 0;
}


Task *
task_get() {
	Task* task_aux;
	int i;
	fprintf(stderr, "task_get()\n");
	pthread_mutex_lock(&mutex);
	task_aux = NULL;
	for(i=0; i < max_tasks; i++) {
		if (tasks[i] != NULL) {
			task_aux = tasks[i];
			tasks[i] = NULL;
			current_tasks--;
			pthread_mutex_unlock(&mutex);
			break;
		}
	}
	fprintf(stderr, "Saliendo: task_get()\n");
	return task_aux;

}

void
task_parse_file_info(char* str_from, Task* task) {
    int pos;
    char* aux_s;
    char* str_size;

    fprintf(stderr, "task_parse_file_info(%s)\n", str_from);
    // allocate space for an int
    str_size = (char*)malloc(sizeof(int));

    aux_s = str_from;
    pos = 0;
    while(aux_s[pos] != ':') {
        pos++;
    }

    memccpy(task->filename, aux_s, ':', sizeof(task->filename));
    fprintf(stderr, "Fuera 1\n");
    task->filename[pos] = '\0';
    fprintf(stderr, "Fuera 2\n");

    aux_s = index(str_from, ':') + 1;
    memcpy(str_size, aux_s, strlen(aux_s));
    str_size[strlen(aux_s)] = '\0';
    task->total = atoi(str_size);

    fprintf(stderr, "Saliendo: task_parse_file_info()\n");
}


void
task_parse_list_message(char *message) {
	Task* task;
	char* ptr;
	char* aux;
	char* ip;

	task = NULL;
	//task = (Task*)malloc(sizeof(Task));
	fprintf(stderr, "task_parse_list_message()\n");

	aux = strdup(message);
	ip = strtok(aux, "@");

	while(*(message++) != '@') {
	}

	ptr = strtok(message, ";");
	while(ptr != NULL) {
		task = (Task*)malloc(sizeof(Task));
		task_parse_file_info(ptr, task);
		memcpy(task->ip, ip, sizeof(task->ip));
		// Agregar el trabajo
		task_add(task);

		ptr = strtok(NULL, ";");
	}
	fprintf(stderr, "Saliendo: task_parse_list_message()\n");
}
