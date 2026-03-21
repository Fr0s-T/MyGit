#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "services.h"

enum e_services_constants {
    DEFAULT_DIR_MODE = 0755,
    PATH_JOIN_EXTRA_CHARS = 2,
};

/*
** Creates a directory at the given path.
** Returns:
**  0  on success
** -1  on failure
*/
int create_directory(const char *path) {
    if (mkdir(path, DEFAULT_DIR_MODE) == 0) {
        printf("\ncreating %s\n", path);
        return 0;
    }

    switch (errno) {
        case EACCES:
            printf("\nPermission denied: %s\n", path);
            break;
        case EEXIST:
            printf("\nDirectory already exists: %s\n", path);
            break;
        case ENOENT:
            printf("\nParent directory does not exist: %s\n", path);
            break;
        case ENAMETOOLONG:
            printf("\nPath too long: %s\n", path);
            break;
        default:
            perror("mkdir");
            break;
    }

    return -1;
}

/*
** Builds a new heap-allocated path of the form:
** base/extension
**
** The caller owns the returned memory and must free it.
*/
char *generate_path(const char *base, const char *extension) {
    size_t len = strlen(base) + strlen(extension) + PATH_JOIN_EXTRA_CHARS;

    char *path = malloc(len);
    if (path == NULL) {
        perror("malloc");
        return NULL;
    }

    snprintf(path, len, "%s/%s", base, extension);
    return path;
}

/*
** Creates an empty file at the given path.
** Returns:
**  0  on success
** -1  on failure
*/
int create_empty_file(const char *file_path_with_name_included) {
    FILE *new_file = fopen(file_path_with_name_included, "w");

    if (new_file == NULL) {
        printf("\nCouldn't create file: %s\n", file_path_with_name_included);
        return -1;
    }

    fclose(new_file);
    printf("\ncreating %s\n", file_path_with_name_included);
    return 0;
}

char *normalize_path(char *generate_path, char *cwd) {
    size_t root_len;
    char *trimmed;

    if (!generate_path || !cwd)
        return (NULL);

    root_len = strlen(cwd);

    if (strncmp(generate_path, cwd, root_len) != 0)
        return (strdup(generate_path));

    /* Index entries stay repo-relative so they can be reused from other machines. */
    if (generate_path[root_len] == '/')
        root_len++;

    trimmed = strdup(generate_path + root_len);
    return (trimmed);
}
