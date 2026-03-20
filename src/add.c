#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <linux/limits.h>

#include "../include/add.h"
#include "../include/add_creating_blob_and_indexing.h"
#include "../include/add_traversal.h"
#include "../include/file_data.h"
#include "../include/hash.h"

static int input_check(int argc, char **argv);
static void destroy_file_list(file_data **files, int len_files);
static int filter_unchanged_files(file_data **files, int *len_files, const char *cwd);
static int read_index_hash_for_path(const char *cwd, const char *path,
    char out[SHA1_HEX_BUFFER_SIZE]);

int add(int argc, char **argv) {
    char cwd[PATH_MAX];
    file_data **files = NULL;
    int len_files = 0;
    int status;

    if (input_check(argc, argv) == -1) {
        return -1;
    }

    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        printf("[add] failed to get working dir\n");
        return -1;
    }

    if (traverse_directory(cwd, &files, &len_files, cwd) == -1) {
        printf("[add] traverse_directory failed\n");
        destroy_file_list(files, len_files);
        return -1;
    }
    if (filter_unchanged_files(files, &len_files, cwd) == -1) {
        printf("[add] failed to compare files against index\n");
        destroy_file_list(files, len_files);
        return -1;
    }
    if (len_files == 0) {
        printf("[add] nothing changed\n");
        destroy_file_list(files, len_files);
        return 0;
    }

    status = create_blob_and_indexing(files, len_files, cwd);
    destroy_file_list(files, len_files);
    if (status != 0) {
        printf("[add] create_blob_and_indexing failed\n");
        return -1;
    }
    printf("[add] added %d file(s)\n", len_files);
    return 0;
}

static int input_check(int argc, char **argv) {
    if (argc > 3) {
        printf("\nToo many args\n");
        return -1;
    }

    if (argc < 3) {
        printf("\nMissing add target\n");
        return -1;
    }

    if (strcmp(argv[2], ".") == 0) {
        return 0;
    }

    printf("\nNot supported or wrong arg\n");
    return -1;
}

static void destroy_file_list(file_data **files, int len_files) {
    if (files == NULL) {
        return;
    }
    for (int i = 0; i < len_files; i++) {
        file_data_destroy(files[i]);
    }
    free(files);
}

static int filter_unchanged_files(file_data **files, int *len_files, const char *cwd) {
    int write_index;

    if (files == NULL || len_files == NULL || cwd == NULL) {
        return (-1);
    }
    write_index = 0;
    for (int read_index = 0; read_index < *len_files; read_index++) {
        char tracked_hash[SHA1_HEX_BUFFER_SIZE];
        int lookup_status;

        lookup_status = read_index_hash_for_path(cwd, files[read_index]->path, tracked_hash);
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

static int read_index_hash_for_path(const char *cwd, const char *path, char out[SHA1_HEX_BUFFER_SIZE]) {
    char index_path[PATH_MAX];
    FILE *index_file;
    char line[PATH_MAX + SHA1_HEX_BUFFER_SIZE + 8];

    if (cwd == NULL || path == NULL || out == NULL) {
        return (-1);
    }
    if (snprintf(index_path, sizeof(index_path), "%s/.mygit/index", cwd) >= (int)sizeof(index_path)) {
        return (-1);
    }
    index_file = fopen(index_path, "r");
    if (index_file == NULL) {
        return (0);
    }
    while (fgets(line, sizeof(line), index_file) != NULL) {
        char *tab;
        char *line_end;
        size_t path_len;
        size_t hash_len;

        tab = strchr(line, '\t');
        if (tab == NULL) {
            continue;
        }
        line_end = strpbrk(tab + 1, "\r\n");
        if (line_end == NULL) {
            line_end = line + strlen(line);
        }
        path_len = (size_t)(tab - line);
        hash_len = (size_t)(line_end - (tab + 1));
        if (strlen(path) == path_len && strncmp(line, path, path_len) == 0) {
            if (hash_len >= SHA1_HEX_BUFFER_SIZE) {
                fclose(index_file);
                return (-1);
            }
            memcpy(out, tab + 1, hash_len);
            out[hash_len] = '\0';
            fclose(index_file);
            return (1);
        }
    }
    fclose(index_file);
    return (0);
}
