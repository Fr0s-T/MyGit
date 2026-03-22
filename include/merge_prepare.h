#ifndef MYGIT_MERGE_PREPARE_H
#define MYGIT_MERGE_PREPARE_H

#include "checkout_entry.h"

typedef enum e_merge_mode {
    /* Report conflicts and stop instead of auto-picking one side. */
    MERGE_MODE_NONE,
    /* Prefer the incoming branch's version when both sides changed. */
    MERGE_MODE_INCOMING,
    /* Prefer the current branch's version when both sides changed. */
    MERGE_MODE_CURRENT
} merge_mode;

/*
** Validates merge CLI usage and returns a borrowed branch name from argv.
*/
int merge_input_check(int argc, char **argv, merge_mode *mode,
    char **branch_name);

/*
** Reads `.mygit/HEAD` and the current branch tip.
**
** Outputs on success:
** - current_ref_path: heap string like `.mygit/refs/heads/main`
** - current_commit_hash: heap string with the current tip hash, possibly empty
**
** Ownership:
** - caller owns both strings and must free them
*/
int merge_read_current_state(char **current_ref_path, char **current_commit_hash);

/*
** Asks whether to use a fast-forward instead of forcing a merge commit.
**
** Returns:
** - 1 to use fast-forward
** - 0 to continue with a non-fast-forward merge
** - -1 on input failure
*/
int merge_prompt_use_fast_forward(const char *branch_name, merge_mode mode);

/*
** Materializes the tracked snapshot for a commit into checkout_entry values.
**
** Ownership:
** - caller owns the returned entry array and must free it with
**   checkout_destroy_entries()
*/
int merge_collect_entries_from_commit(const char *commit_hash,
    checkout_entry ***entries, int *entry_count);

/*
** Returns 1 if ancestor_hash is reachable from commit_hash, 0 if not, and -1
** on read/allocation failure.
*/
int merge_commit_is_ancestor(const char *ancestor_hash, const char *commit_hash);

/*
** Finds the best common ancestor between two commits.
**
** Output on success:
** - merge_base_hash: heap string containing the selected merge-base hash
**   or an empty string when no common ancestor is found
**
** Ownership:
** - caller owns merge_base_hash and must free it
*/
int merge_find_merge_base(const char *left_hash, const char *right_hash,
    char **merge_base_hash);

/*
** Builds the default merge commit message `Merge branch '<name>'`.
**
** Ownership:
** - caller owns the returned string and must free it
*/
char *merge_build_default_message(const char *branch_name);

#endif
