#ifndef MYGIT_ADD_H
#define MYGIT_ADD_H

/*
** CLI entry point for `mygit add .`.
**
** Parameters:
** - argc/argv: command-line arguments from main()
**
** Returns:
** - 0 on success, including the "nothing changed" case
** - -1 on invalid usage or runtime failure
**
** Ownership:
** - borrows argv for the duration of the call
** - does not retain any caller-provided pointers
*/
int add(int argc, char **argv);

#endif
