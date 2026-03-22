#include <linux/limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "add_snapshot.h"
#include "add_traversal.h"
#include "gitignore.h"
#include "hash.h"
#include "helpers/file_io.h"

static int filter_unchanged_files(file_data **files, int *len_files, const char *cwd);

int add_collect_changed_files(const char *cwd, file_data ***files_out, int *len_out) {
    file_data **files;
    int len_files;
    gitignore ignore;

    if (cwd == NULL || files_out == NULL || len_out == NULL) {
        return (-1);
    }
    *files_out = NULL;
    *len_out = 0;
    files = NULL;
    len_files = 0;
    ignore.rules = NULL;
    ignore.count = 0;
    ignore.index_path = NULL;
    if (gitignore_load(cwd, &ignore) == -1) {
        return (-1);
    }
    if (traverse_directory(cwd, &files, &len_files, (char *)cwd,
            &ignore) == -1) {
        gitignore_destroy(&ignore);
        add_destroy_file_list(files, len_files);
        return (-1);
    }
    if (filter_unchanged_files(files, &len_files, cwd) == -1) {
        gitignore_destroy(&ignore);
        add_destroy_file_list(files, len_files);
        return (-1);
    }
    gitignore_destroy(&ignore);
    *files_out = files;
    *len_out = len_files;
    return (0);
}

void add_destroy_file_list(file_data **files, int len_files) {
    if (files == NULL) {
        return;
    }
    for (int i = 0; i < len_files; i++) {
        file_data_destroy(files[i]);
    }
    free(files);
}

static int filter_unchanged_files(file_data **files, int *len_files, const char *cwd) {
    char index_path[PATH_MAX];
    int write_index;

    if (len_files == NULL || cwd == NULL || *len_files < 0) {
        return (-1);
    }
    if (*len_files == 0) {
        return (0);
    }
    if (files == NULL) {
        return (-1);
    }
    if (snprintf(index_path, sizeof(index_path), "%s/.mygit/index", cwd)
            >= (int)sizeof(index_path)) {
        return (-1);
    }
    write_index = 0;
    for (int read_index = 0; read_index < *len_files; read_index++) {
        char tracked_hash[SHA1_HEX_BUFFER_SIZE];
        int lookup_status;

        lookup_status = file_io_read_index_hash(index_path, files[read_index]->path, tracked_hash);
        if (lookup_status == -1) {
            return (-1);
        }
        if (lookup_status == 1 && strcmp(tracked_hash, files[read_index]->hash) == 0) {
            file_data_destroy(files[read_index]);
            continue;
        }
        files[write_index] = files[read_index];
        write_index++;
    }
    *len_files = write_index;
    return (0);
}
