#ifndef MYGIT_CHECKOUT_PREPARE_H
#define MYGIT_CHECKOUT_PREPARE_H

/*
** Validates checkout CLI usage and returns the target branch name borrowed
** from argv.
**
** Ownership:
** - branch_name points into argv and must not be freed
*/
int checkout_input_check(int argc, char **argv, char **branch_name);

/*
** Returns 1 when a branch ref exists, 0 when it does not, and -1 on failure.
*/
int checkout_branch_exists(const char *branch_name);

/*
** Checks that the working tree matches the current branch tip closely enough
** to allow checkout/reset/merge operations.
**
** Returns:
** - 0 when the repo is considered clean enough
** - -1 when tracked changes, missing tracked files, or read failures are found
*/
int checkout_repo_is_up_to_date_with_branch(void);

/*
** Resolves a branch name to its ref path and current commit hash.
**
** Outputs on success:
** - target_ref_path: heap string like `.mygit/refs/heads/main`
** - target_commit_hash: heap string with the branch tip hash, possibly empty
**
** Ownership:
** - caller owns both output strings and must free them
*/
int checkout_read_target_commit(const char *branch_name,
    char **target_ref_path, char **target_commit_hash);

/*
** Resolves a commit hash to its root tree hash and on-disk tree-object path.
**
** Outputs on success:
** - root_hash: heap string containing the tree hash
** - root_path: heap string containing `.mygit/objects/<tree-hash>`
**
** Ownership:
** - caller owns both output strings and must free them
*/
int checkout_read_target_root(const char *target_commit_hash,
    char **root_hash, char **root_path);

/*
** Replaces `.mygit/HEAD` with a plain-text ref path.
**
** Ownership:
** - borrows target_ref_path
*/
int checkout_update_head_ref(const char *target_ref_path);

#endif
