#ifndef MY_GIT_COMMIT
#define MY_GIT_COMMIT

/*
** CLI entry point for `mygit commit -m "message"`.
**
** Returns:
** - 0 on success, including the "nothing changed" case
** - -1 on invalid usage or runtime failure
**
** Ownership:
** - borrows argv for the duration of the call
*/
int commit(int argc, char **argv);

#endif
