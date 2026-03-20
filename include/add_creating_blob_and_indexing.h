#ifndef MYGIT_ADD_CREATING_BLOB_AND_INDEXING
#define MYGIT_ADD_CREATING_BLOB_AND_INDEXING

#include "file_data.h"

int create_blob_and_indexing(file_data **files, int len_file, char *cwd);
char *create_git_obj_dir(char *cwd);

#endif
