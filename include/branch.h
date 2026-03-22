#ifndef MY_GIT_BRANCH_H
#define MY_GIT_BRANCH_H

/*
** CLI entry point for `mygit branch`.
**
** Supported forms:
** - `mygit branch`
** - `mygit branch <name>`
** - `mygit branch -d <name>`
**
** Returns:
** - 0 on success
** - -1 on invalid usage or runtime failure
**
** Ownership:
** - borrows argv for the duration of the call
*/
int branch(int argc, char **argv);

/*
** Loads branch names from `.mygit/refs/heads`.
**
** Outputs on success:
** - names_out: heap array of heap-allocated branch-name strings
** - count_out: number of names in the array
**
** Ownership:
** - caller owns the returned array and strings
** - free them with branch_destroy_names()
*/
int branch_load_names(char ***names_out, int *count_out);

/*
** Frees the result produced by branch_load_names().
*/
void branch_destroy_names(char **names, int count);

#endif
