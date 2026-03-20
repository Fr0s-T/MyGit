#include <stdlib.h>
#include <string.h>
#include "../include/file_data.h"

file_data *file_data_create(const char *path, const char *hash) {
    file_data *file = malloc(sizeof(file_data));
    if (file == NULL) {
        return NULL;
    }

    file->path = NULL;
    file->hash = NULL;

    if (path == NULL) {
        free(file);
        return NULL;
    }

    file->path = malloc(strlen(path) + 1);
    if (file->path == NULL) {
        free(file);
        return NULL;
    }
    strcpy(file->path, path);

    if (hash != NULL) {
        file->hash = malloc(strlen(hash) + 1);
        if (file->hash == NULL) {
            free(file->path);
            free(file);
            return NULL;
        }
        strcpy(file->hash, hash);
    }

    return file;
}

void file_data_destroy(file_data *file) {
    if (file == NULL) {
        return;
    }

    free(file->path);
    free(file->hash);
    free(file);
}