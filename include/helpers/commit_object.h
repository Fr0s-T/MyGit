#ifndef MYGIT_HELPERS_COMMIT_OBJECT_H
#define MYGIT_HELPERS_COMMIT_OBJECT_H

#include "../hash.h"
#include "../node.h"

#define COMMIT_NOTHING_TO_DO 1

typedef struct s_commit_object_info {
    char *tree_hash;
    char *branch_name;
    char *time_value;
    char **parent_hashes;
    int parent_count;
} commit_object_info;

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
** Persists a new commit with explicit parent hashes.
**
** Notes:
** - used for merge-style commits that need more than one parent
** - unlike accept_commit(), this does not abort just because the tree hash
**   matches the current branch tip
*/
int accept_commit_with_parents(node *root, const char *commit_msg,
    char **parent_hashes, int parent_count,
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

/*
** Loads structured metadata from a commit object.
**
** Output on success:
** - fills tree/branch/time
** - collects zero or more parent hashes
*/
int commit_object_read_info(const char *commit_hash, commit_object_info *info);

/*
** Frees all heap-owned fields on a commit_object_info.
*/
void commit_object_destroy_info(commit_object_info *info);

#endif
