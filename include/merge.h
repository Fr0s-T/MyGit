#ifndef MYGIT_MERGE_H
#define MYGIT_MERGE_H

/*
** CLI entry point for `mygit merge`.
**
** Supported forms:
** - `mygit merge <branch>`
** - `mygit merge -i <branch>`
** - `mygit merge -c <branch>`
**
** Returns:
** - 0 on success
** - -1 on invalid usage, conflicts in default mode, or apply failure
**
** Ownership:
** - borrows argv for the duration of the call
*/
int merge_cmd(int argc, char **argv);

#endif
