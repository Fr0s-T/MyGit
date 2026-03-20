#include "../include/my_includes.h"

enum e_traversal_constants {
    PATH_HASH_BUFFER_SIZE = SHA1_HEX_BUFFER_SIZE,
};

static int should_skip_entry(const char *entry_name);
static int process_directory_entry(const char *parent_dir_path, struct dirent *entry,
    file_data ***files, int *len_files, char *cwd);
static int resolve_entry_path(const char *parent_dir_path, const char *entry_name,
    char **entry_path, char **resolved_path);
static int get_entry_stat(const char *path, struct stat *entry_stat);
static int append_file_data(file_data ***files, int *len_files, char *resolved_path, char *cwd);

int traverse_directory(const char *directory_path, file_data ***files, int *len_files, char *cwd) {
    DIR *current_dir;
    struct dirent *entry;

    current_dir = opendir(directory_path);
    if (current_dir == NULL) {
        printf("\nFailed to open dir: %s\n", directory_path);
        return -1;
    }

    while ((entry = readdir(current_dir)) != NULL) {
        if (should_skip_entry(entry->d_name)) {
            continue;
        }

        if (process_directory_entry(directory_path, entry, files, len_files, cwd) == -1) {
            closedir(current_dir);
            return -1;
        }
    }

    closedir(current_dir);
    return 0;
}

static int should_skip_entry(const char *entry_name) {
    /* Keep repository internals and build output out of the tracked file list. */
    if (strcmp(entry_name, ".") == 0 ||
        strcmp(entry_name, "..") == 0 ||
        strcmp(entry_name, ".mygit") == 0 ||
        strcmp(entry_name, ".git") == 0 ||
        strcmp(entry_name, "out") == 0 ||
        strcmp(entry_name, ".vscode") == 0) {
        return 1;
    }

    return 0;
}

static int resolve_entry_path(const char *parent_dir_path, const char *entry_name,
    char **entry_path, char **resolved_path) {
    *entry_path = generate_path(parent_dir_path, entry_name);
    if (*entry_path == NULL) {
        return -1;
    }

    *resolved_path = realpath(*entry_path, NULL);
    if (*resolved_path == NULL) {
        printf("[realpath error] Couldn't retrieve the path for: %s\n", entry_name);
        printf("[realpath error] current_path: %s\n", *entry_path);
        free(*entry_path);
        *entry_path = NULL;
        return -1;
    }

    return 0;
}

static int get_entry_stat(const char *path, struct stat *entry_stat) {
    if (stat(path, entry_stat) == -1) {
        perror("stat");
        return -1;
    }

    return 0;
}

static int append_file_data(file_data ***files, int *len_files, char *resolved_path, char *cwd) {
    file_data **tmp;
    char *normalized_path;

    tmp = realloc(*files, (*len_files + 1) * sizeof(file_data *));
    if (tmp == NULL) {
        return -1;
    }

    *files = tmp;

    char hash[PATH_HASH_BUFFER_SIZE];
    if (hash_file_sha1(resolved_path, hash) != 0) {
        return -1;
    }

    normalized_path = normalize_path(resolved_path, cwd);
    if (normalized_path == NULL) {
        return -1;
    }
    (*files)[*len_files] = file_data_create(normalized_path, hash);
    free(normalized_path);
    if ((*files)[*len_files] == NULL) {
        return -1;
    }

    (*len_files)++;
    return 0;
}

static int process_directory_entry(const char *parent_dir_path, struct dirent *entry,
    file_data ***files, int *len_files, char *cwd) {
    char *entry_path = NULL;
    char *resolved_path = NULL;
    struct stat entry_stat;

    if (resolve_entry_path(parent_dir_path, entry->d_name, &entry_path, &resolved_path) == -1) {
        return -1;
    }

    if (get_entry_stat(resolved_path, &entry_stat) == -1) {
        free(entry_path);
        free(resolved_path);
        return -1;
    }

    if (S_ISDIR(entry_stat.st_mode)) {
        if (traverse_directory(resolved_path, files, len_files, cwd) == -1) {
            free(entry_path);
            free(resolved_path);
            return -1;
        }
    }
    else if (S_ISREG(entry_stat.st_mode)) {
        if (append_file_data(files, len_files, resolved_path, cwd) == -1) {
            free(entry_path);
            free(resolved_path);
            return -1;
        }
    }

    free(entry_path);
    free(resolved_path);
    return 0;
}
