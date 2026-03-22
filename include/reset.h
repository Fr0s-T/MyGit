#ifndef MYGIT_RESET_H
#define MYGIT_RESET_H

/*
** CLI entry point for `mygit reset -r <commit_hash>`.
**
** Returns:
** - 0 on success
** - -1 on invalid usage, safety checks, or apply failures
**
** Ownership:
** - borrows argv for the duration of the call
*/
int reset_cmd(int argc, char **argv);

#endif
