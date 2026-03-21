#ifndef MYGIT_HELPERS_COMMIT_OBJECT_H
#define MYGIT_HELPERS_COMMIT_OBJECT_H

#include "../hash.h"
#include "../node.h"

#define COMMIT_NOTHING_TO_DO 1

/*
** Persists a new commit for the current HEAD branch.
**
** Inputs:
** - root: built tree root, with root->hash already computed
** - commit_msg: commit message to store
**
** Output on success:
** - out: receives the new commit hash
*/
int accept_commit(node *root, const char *commit_msg,
    char out[SHA1_HEX_BUFFER_SIZE]);

/*
** Reads the tree hash stored in a commit object.
**
** Inputs:
** - commit_hash: commit object hash from refs/heads or parent field
**
** Output on success:
** - tree_hash: heap-allocated tree hash string
**
** Caller owns tree_hash and must free it.
*/
int read_commit_tree_hash(const char *commit_hash, char **tree_hash);

#endif
