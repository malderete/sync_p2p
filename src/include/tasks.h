#ifndef TASKS_H_
#define TASKS_H_
#define FILE_SIZE 50
#define STOP_MESSAGE "SHUTDOWN"


typedef struct task_struct Task;

struct task_struct {
    char ip[16];
    char filename[FILE_SIZE];
    int total;

};


void task_parse_list_message(char* message);
void task_parse_file_info(char* str_from, Task* task);
int tasks_initialize();
int task_add(Task* task);
Task* task_get();
int task_size();

#endif /* TASKS_H_ */
