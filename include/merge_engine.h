#ifndef MYGIT_MERGE_ENGINE_H
#define MYGIT_MERGE_ENGINE_H

#include "checkout_entry.h"
#include "merge_prepare.h"

/*
** Resolves a three-way merge over tracked file entries.
**
** Parameters:
** - base_entries: merge-base snapshot
** - current_entries: current branch snapshot
** - target_entries: branch being merged in
** - mode: conflict policy when both sides changed differently
**
** Outputs on success:
** - merged_entries: heap array of cloned resolved entries
** - merged_count: number of resolved entries
** - conflict_paths: heap array of conflicting repo-relative paths
** - conflict_count: number of conflicts
**
** Behavior:
** - in MERGE_MODE_NONE, conflicts are reported but merged_entries is cleared
** - in MERGE_MODE_INCOMING / MERGE_MODE_CURRENT, conflicts are auto-resolved
**
** Ownership:
** - caller owns merged_entries and conflict_paths on success
** - free merged_entries with checkout_destroy_entries()
** - free conflict_paths with merge_destroy_conflicts()
** - borrows all input entry arrays
*/
int merge_build_entries(checkout_entry **base_entries, int base_count,
    checkout_entry **current_entries, int current_count,
    checkout_entry **target_entries, int target_count, merge_mode mode,
    checkout_entry ***merged_entries, int *merged_count,
    char ***conflict_paths, int *conflict_count);

/*
** Frees a conflict path list produced by merge_build_entries().
*/
void merge_destroy_conflicts(char **conflict_paths, int conflict_count);

#endif
