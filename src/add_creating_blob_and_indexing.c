#include "../include/my_includes.h"

enum e_blob_constants {
    BLOB_COPY_BUFFER_SIZE = 4096,
    INDEX_LINE_EXTRA_SPACE = 128,
};

int create_blob(file_data *file, char *git_obj_path);
char *create_git_obj_dir(char *cwd);
int blob_exists(char *file_hash, char *git_obj_path);
int copy_file_content(char *src_path, char *dst_path);
int update_index(file_data *file, char *cwd);

int create_blob_and_indexing(file_data **files, int len_file, char *cwd) {
    char *git_obj_path;
    int status;

    status = 0;
    git_obj_path = create_git_obj_dir(cwd);
    if (git_obj_path == NULL) {
        return (-1);
    }
    for (int i = 0; i < len_file; i++) {
        file_data *file = files[i];

        if (create_blob(file, git_obj_path) != 0) {
            status = -1;
            continue;
        }
        if (update_index(file, cwd) != 0) {
            status = -1;
        }
    }
    free(git_obj_path);
    return (status);
}

char *create_git_obj_dir(char *cwd) {
    return (generate_path(cwd, ".mygit/objects"));
}

int create_blob(file_data *file, char *git_obj_path) {
    char blob_path[PATH_MAX];
    int exists_status;

    exists_status = blob_exists(file->hash, git_obj_path);
    if (exists_status == 0) {
        return (0);
    }
    if (exists_status == -1) {
        return (-1);
    }
    if (snprintf(blob_path, sizeof(blob_path), "%s/%s", git_obj_path, file->hash) >= (int)sizeof(blob_path)) {
        return (-1);
    }
    if (copy_file_content(file->path, blob_path) != 0) {
        return (-1);
    }
    return (0);
}

int copy_file_content(char *src_path, char *dst_path) {
    FILE *src_file;
    FILE *dst_file;
    char buffer[BLOB_COPY_BUFFER_SIZE];
    size_t bytes_read;
    size_t bytes_written;
    size_t total_written;

    total_written = 0;
    src_file = fopen(src_path, "rb");
    if (src_file == NULL) {
        return (-1);
    }
    dst_file = fopen(dst_path, "wb");
    if (dst_file == NULL) {
        fclose(src_file);
        return (-1);
    }
    bytes_read = fread(buffer, 1, sizeof(buffer), src_file);
    while (bytes_read > 0) {
        bytes_written = fwrite(buffer, 1, bytes_read, dst_file);
        if (bytes_written != bytes_read) {
            fclose(src_file);
            fclose(dst_file);
            remove(dst_path);
            return (-1);
        }
        total_written += bytes_written;
        bytes_read = fread(buffer, 1, sizeof(buffer), src_file);
    }
    if (ferror(src_file)) {
        fclose(src_file);
        fclose(dst_file);
        remove(dst_path);
        return (-1);
    }
    fclose(src_file);
    fclose(dst_file);
    return (0);
}

int blob_exists(char *file_hash, char *git_obj_path) {
    DIR *current_dir;
    struct dirent *entry;

    current_dir = opendir(git_obj_path);
    if (current_dir == NULL) {
        return (-1);
    }
    while ((entry = readdir(current_dir)) != NULL) {
        if (strcmp(file_hash, entry->d_name) == 0) {
            closedir(current_dir);
            return (0);
        }
    }
    closedir(current_dir);
    return (1);
}

int update_index(file_data *file, char *cwd) {
    char index_path[PATH_MAX];
    char temp_path[PATH_MAX];
    FILE *index_file;
    FILE *temp_file;
    char line[PATH_MAX + INDEX_LINE_EXTRA_SPACE];
    char existing_path[PATH_MAX];
    char existing_hash[SHA1_HEX_BUFFER_SIZE];
    int found;

    found = 0;
    if (snprintf(index_path, sizeof(index_path), "%s/.mygit/index", cwd) >= (int)sizeof(index_path)) {
        return (-1);
    }
    if (snprintf(temp_path, sizeof(temp_path), "%s/.mygit/index.tmp", cwd) >= (int)sizeof(temp_path)) {
        return (-1);
    }
    index_file = fopen(index_path, "r");
    temp_file = fopen(temp_path, "w");
    if (temp_file == NULL) {
        if (index_file != NULL) {
            fclose(index_file);
        }
        return (-1);
    }
    if (index_file != NULL) {
        /* Rewriting through a temp file keeps the index consistent on partial failures. */
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
            if (path_len == 0 || path_len >= sizeof(existing_path)) {
                continue;
            }
            if (hash_len == 0 || hash_len >= sizeof(existing_hash)) {
                continue;
            }
            memcpy(existing_path, line, path_len);
            existing_path[path_len] = '\0';
            memcpy(existing_hash, tab + 1, hash_len);
            existing_hash[hash_len] = '\0';
            if (strcmp(existing_path, file->path) == 0) {
                fprintf(temp_file, "%s\t%s\n", file->path, file->hash);
                found = 1;
            }
            else {
                fprintf(temp_file, "%s\t%s\n", existing_path, existing_hash);
            }
        }
        fclose(index_file);
    }
    if (found == 0) {
        fprintf(temp_file, "%s\t%s\n", file->path, file->hash);
    }
    fclose(temp_file);
    if (rename(temp_path, index_path) != 0) {
        remove(temp_path);
        return (-1);
    }
    return (0);
}
