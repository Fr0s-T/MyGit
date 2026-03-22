#ifndef MYGIT_CHECKOUT_APPLY_H
#define MYGIT_CHECKOUT_APPLY_H

#include "checkout_entry.h"

/*
** Frees an array of checkout_entry pointers and each owned entry.
**
** Ownership:
** - accepts ownership of entries and its contents
** - safe to call with NULL
*/
void checkout_destroy_entries(checkout_entry **entries, int count);

/*
** Debug helper that prints a labeled list of entries.
**
** Ownership:
** - borrows label and entries
*/
void checkout_print_entries(const char *label, checkout_entry **entries,
    int count);

/*
** Builds the subset of current entries that can safely remain on disk while
** switching to target_entries.
**
** Outputs on success:
** - surviving_entries: heap array of cloned checkout_entry values
** - surviving_count: number of entries in surviving_entries
**
** Ownership:
** - caller owns surviving_entries and must free it with
**   checkout_destroy_entries()
** - borrows target_entries and current_entries
*/
int checkout_build_surviving_entries(checkout_entry ***surviving_entries,
    checkout_entry **target_entries, checkout_entry **current_entries,
    int target_count, int current_count, int *surviving_count);

/*
** Removes tracked files from the working tree that should not survive the
** transition.
**
** Ownership:
** - borrows all entry arrays
** - does not free or retain them
*/
int checkout_purge_non_surviving_current_entries(
    checkout_entry **current_entries, int current_count,
    checkout_entry **surviving_entries, int surviving_count);

/*
** Materializes target tracked files from their blob objects into the working
** tree.
**
** Ownership:
** - borrows target_entries
*/
int checkout_materialize_target_entries(checkout_entry **target_entries,
    int target_count);

/*
** Rewrites `.mygit/index` to exactly match the provided tracked entries.
**
** Ownership:
** - borrows entries
*/
int checkout_rewrite_index_from_entries(checkout_entry **entries, int count);

/*
** Verifies that every entry points to an existing blob object on disk.
**
** Returns:
** - 0 when all referenced objects exist
** - -1 when input is invalid or an object is missing
*/
int checkout_validate_entries_available(checkout_entry **entries, int count);

/*
** Best-effort rollback helper used after checkout/merge/reset apply failures.
**
** Parameters:
** - current_entries: state to restore
** - target_entries: state that may already have been partially applied
** - surviving_entries: subset shared by both states
**
** Ownership:
** - borrows all arrays
** - does not free or retain them
*/
int checkout_restore_state(checkout_entry **current_entries, int current_count,
    checkout_entry **target_entries, int target_count,
    checkout_entry **surviving_entries, int surviving_count);

/*
** Detects whether applying target_entries would overwrite untracked paths in
** the working tree.
**
** Returns:
** - 1 if at least one untracked conflict exists
** - 0 if the transition is safe
** - -1 on invalid input or filesystem failure
*/
int checkout_target_conflicts_with_untracked(checkout_entry **target_entries,
    int target_count, checkout_entry **current_entries, int current_count);

#endif
