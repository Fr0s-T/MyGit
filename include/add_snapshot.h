#ifndef MYGIT_ADD_SNAPSHOT_H
#define MYGIT_ADD_SNAPSHOT_H

#include "file_data.h"

/*
** Collects the set of changed files that `mygit add .` would stage.
**
** Inputs:
** - cwd: absolute repo root / working directory
**
** Outputs on success:
** - files_out: heap array of file_data pointers
** - len_out: number of entries in files_out
**
** Notes:
** - unchanged files already matching `.mygit/index` are filtered out
** - this helper does not write blobs or update the index
** - caller owns the returned file list and must free it with
**   add_destroy_file_list()
*/
int add_collect_changed_files(const char *cwd, file_data ***files_out, int *len_out);

/*
** Frees a file list produced by add_collect_changed_files().
*/
void add_destroy_file_list(file_data **files, int len_files);

#endif
