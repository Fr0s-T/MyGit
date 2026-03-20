#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../include/helpers/file_io.h"

enum e_file_io_constants {
    FILE_IO_COPY_BUFFER_SIZE = 4096,
    FILE_IO_INDEX_LINE_EXTRA = 8,
};

static char *file_io_strdup(const char *value);
static void file_io_trim_line_endings(char *value);

/*
** Reads only the first line from a small text file.
**
** This is useful for files such as:
** - .mygit/HEAD
** - refs/heads/main
** - single-line metadata files
**
** Ownership:
** - allocates *out on success
** - caller must free(*out)
*/
int file_io_read_first_line(const char *path, char **out) {
    FILE *file;
    char buffer[4096];

    if (path == NULL || out == NULL) {
        return (-1);
    }
    file = fopen(path, "r");
    if (file == NULL) {
        return (-1);
    }
    if (fgets(buffer, sizeof(buffer), file) == NULL) {
        if (ferror(file)) {
            fclose(file);
            return (-1);
        }
        fclose(file);
        *out = file_io_strdup("");
        return (*out == NULL ? -1 : 0);
    }
    fclose(file);
    file_io_trim_line_endings(buffer);
    *out = file_io_strdup(buffer);
    if (*out == NULL) {
        return (-1);
    }
    return (0);
}

/*
** Replaces the content of a file with the provided text buffer.
**
** This helper is meant for small text payloads, not streaming large files.
** Typical future uses:
** - updating a branch ref
** - writing HEAD
** - writing simple object payloads
*/
int file_io_write_text(const char *path, const char *content) {
    FILE *file;
    size_t content_len;

    if (path == NULL || content == NULL) {
        return (-1);
    }
    file = fopen(path, "wb");
    if (file == NULL) {
        return (-1);
    }
    content_len = strlen(content);
    if (fwrite(content, 1, content_len, file) != content_len) {
        fclose(file);
        return (-1);
    }
    fclose(file);
    return (0);
}

/*
** Copies raw bytes from one file to another.
**
** This is the generic file-copy helper behind blob-style operations.
** If the write fails after the destination file is created, the partial
** destination file is removed to avoid leaving corrupted output behind.
*/
int file_io_copy_file(const char *src_path, const char *dst_path) {
    FILE *src_file;
    FILE *dst_file;
    char buffer[FILE_IO_COPY_BUFFER_SIZE];
    size_t bytes_read;
    size_t bytes_written;

    if (src_path == NULL || dst_path == NULL) {
        return (-1);
    }
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

/*
** Looks up one path inside a tab-separated index file.
**
** Expected line format:
** relative/path<TAB>sha1hash<NEWLINE>
**
** Return values:
** - 1 if the path exists and out_hash was filled
** - 0 if the path is not present
** - -1 on file or parsing failure
*/
int file_io_read_index_hash(const char *index_path, const char *file_path,
    char out_hash[SHA1_HEX_BUFFER_SIZE]) {
    FILE *index_file;
    char line[4096 + SHA1_HEX_BUFFER_SIZE + FILE_IO_INDEX_LINE_EXTRA];

    if (index_path == NULL || file_path == NULL || out_hash == NULL) {
        return (-1);
    }
    index_file = fopen(index_path, "r");
    if (index_file == NULL) {
        return (-1);
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
        if (strlen(file_path) == path_len && strncmp(line, file_path, path_len) == 0) {
            if (hash_len >= SHA1_HEX_BUFFER_SIZE) {
                fclose(index_file);
                return (-1);
            }
            memcpy(out_hash, tab + 1, hash_len);
            out_hash[hash_len] = '\0';
            fclose(index_file);
            return (1);
        }
    }
    fclose(index_file);
    return (0);
}

/*
** Small local strdup replacement so this helper module does not depend on
** non-standard library variants.
*/
static char *file_io_strdup(const char *value) {
    char *copy;

    if (value == NULL) {
        return (NULL);
    }
    copy = malloc(strlen(value) + 1);
    if (copy == NULL) {
        return (NULL);
    }
    strcpy(copy, value);
    return (copy);
}

/*
** Removes trailing newline-style characters after reading a text line.
*/
static void file_io_trim_line_endings(char *value) {
    size_t len;

    if (value == NULL) {
        return ;
    }
    len = strlen(value);
    while (len > 0 && (value[len - 1] == '\n' || value[len - 1] == '\r')) {
        value[len - 1] = '\0';
        len--;
    }
}
