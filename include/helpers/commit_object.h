#ifndef MYGIT_HELPERS_COMMIT_OBJECT_H
#define MYGIT_HELPERS_COMMIT_OBJECT_H

#include "../hash.h"
#include "../node.h"

#define COMMIT_NOTHING_TO_DO 1

int accept_commit(node *root, const char *commit_msg,
    char out[SHA1_HEX_BUFFER_SIZE]);

#endif
