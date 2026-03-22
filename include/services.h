#ifndef MYGIT_SERVICES_H
#define MYGIT_SERVICES_H

/*
** Creates one directory path.
**
** Returns:
** - 0 on success
** - -1 on failure
*/
int create_directory(const char *path);

/*
** Joins two path segments as `base/extension`.
**
** Ownership:
** - caller owns the returned string and must free it
*/
char *generate_path(const char *base, const char *extension);

/*
** Creates or truncates an empty file at file_path_with_name_included.
*/
int create_empty_file(const char *file_path_with_name_included);

/*
** Converts an absolute path under cwd into a repo-relative path.
**
** Returns:
** - heap-allocated normalized path on success
** - NULL on invalid input or allocation failure
**
** Ownership:
** - caller owns the returned string and must free it
** - borrows generate_path and cwd
*/
char *normalize_path(char *generate_path, char *cwd);

#endif
