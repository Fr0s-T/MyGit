#ifndef MYGIT_ADD_TRAVERSAL_H
#define MYGIT_ADD_TRAVERSAL_H

#include "file_data.h"
#include "gitignore.h"

/*
** Recursively walks a working-tree directory and appends regular files to a
** file_data list.
**
** Parameters:
** - directory_path: absolute or resolved directory to scan
** - files: in/out heap array of file_data pointers; may start as NULL
** - len_files: in/out count matching files
** - cwd: absolute repository root used to normalize paths
** - ignore: loaded ignore rules to consult while traversing
**
** Returns:
** - 0 on success
** - -1 on filesystem, hashing, or allocation failure
**
** Ownership:
** - may realloc *files and append new heap-allocated file_data entries
** - caller owns the accumulated list and should destroy it, even after
**   a partial failure
** - borrows cwd and ignore
*/
int traverse_directory(const char *directory_path, file_data ***files,
    int *len_files, char *cwd, const gitignore *ignore);

#endif
