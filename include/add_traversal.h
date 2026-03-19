#ifndef MYGIT_ADD_TRAVERSAL_H
#define MYGIT_ADD_TRAVERSAL_H

#include "file_data.h"

int traverse_directory(const char *directory_path, file_data ***files, int *len_files, char *cwd);

#endif