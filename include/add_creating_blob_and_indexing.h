#ifndef MYGIT_ADD_CREATING_BLOB_AND_INDEXING
#define MYGIT_ADD_CREATING_BLOB_AND_INDEXING

#include "file_data.h"

/*
** Writes blob objects for each file and updates `.mygit/index`.
**
** Parameters:
** - files: array of file_data pointers to stage
** - len_file: number of entries in files
** - cwd: absolute repository root path
**
** Returns:
** - 0 when all blobs/index updates succeed
** - -1 if any blob creation or index rewrite fails
**
** Ownership:
** - borrows files, each file_data entry, and cwd
** - caller keeps ownership and must free files separately
*/
int create_blob_and_indexing(file_data **files, int len_file, char *cwd);

/*
** Builds `<cwd>/.mygit/objects`.
**
** Returns:
** - heap-allocated path string on success
** - NULL on allocation failure
**
** Ownership:
** - caller owns the returned string and must free it
*/
char *create_git_obj_dir(char *cwd);

#endif
