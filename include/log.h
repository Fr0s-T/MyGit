#ifndef MY_GIT_LOG_H
#define MY_GIT_LOG_H

/*
** CLI entry point for `mygit log`.
**
** Returns:
** - 0 on success, including the "no commits yet" case
** - -1 on invalid usage or object-read failure
**
** Ownership:
** - borrows argv for the duration of the call
*/
int log_cmd(int argc, char **argv);

#endif
