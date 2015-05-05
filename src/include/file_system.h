#ifndef FILE_SYSTEM_H_
#define FILE_SYSTEM_H_


typedef struct file_struct FileInfo;
struct file_struct {
	char *filename;
    char *abs_path;
	int bytes;
    void *content;
};

void filesystem_load(const char *dir_name);
void _list_dir(const char *dir_name, int *size);
void serialize_files(char *buffer);
void _filesystem_print(int size);
FileInfo* file_system_get(char* filename);


#endif /* FILE_SYSTEM_H_ */
