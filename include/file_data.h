#ifndef FILE_DATA_H
#define FILE_DATA_H

typedef struct file_data {
    /* Repo-relative file path, owned by the struct. */
    char *path;
    /* Blob hash string, owned by the struct. May be NULL in some callers. */
    char *hash;
} file_data;

/*
** Creates one heap-owned file_data record by copying path/hash strings.
**
** Ownership:
** - borrows path and hash
** - caller owns the returned struct and must free it with file_data_destroy()
*/
file_data *file_data_create(const char *path, const char *hash);

/*
** Frees one file_data record and its owned strings.
*/
void file_data_destroy(file_data *file);

#endif
