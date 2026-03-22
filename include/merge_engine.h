#ifndef MYGIT_MERGE_ENGINE_H
#define MYGIT_MERGE_ENGINE_H

#include "checkout_entry.h"
#include "merge_prepare.h"

int merge_build_entries(checkout_entry **base_entries, int base_count,
    checkout_entry **current_entries, int current_count,
    checkout_entry **target_entries, int target_count, merge_mode mode,
    checkout_entry ***merged_entries, int *merged_count,
    char ***conflict_paths, int *conflict_count);
void merge_destroy_conflicts(char **conflict_paths, int conflict_count);

#endif
