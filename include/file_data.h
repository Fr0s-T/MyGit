#ifndef FILE_DATA_H
#define FILE_DATA_H

typedef struct file_data {
    char *path;
    char *hash;
} file_data;

file_data *file_data_create(const char *path, const char *hash);
void file_data_destroy(file_data *file);

#endif