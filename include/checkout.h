#ifndef MYGIT_CHECKOUT_H
#define MYGIT_CHECKOUT_H

/*
** CLI entry point for `mygit checkout <branch>`.
**
** Returns:
** - 0 on success
** - -1 on invalid usage, safety checks, or apply failures
**
** Ownership:
** - borrows argv for the duration of the call
*/
int checkout(int argc, char **argv);

#endif
