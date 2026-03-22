#ifndef MYGIT_CHECKOUT_APPLY_H
#define MYGIT_CHECKOUT_APPLY_H

#include "checkout_entry.h"

void checkout_destroy_entries(checkout_entry **entries, int count);
void checkout_print_entries(const char *label, checkout_entry **entries,
    int count);
int checkout_build_surviving_entries(checkout_entry ***surviving_entries,
    checkout_entry **target_entries, checkout_entry **current_entries,
    int target_count, int current_count, int *surviving_count);
int checkout_purge_non_surviving_current_entries(
    checkout_entry **current_entries, int current_count,
    checkout_entry **surviving_entries, int surviving_count);
int checkout_materialize_target_entries(checkout_entry **target_entries,
    int target_count);
int checkout_rewrite_index_from_entries(checkout_entry **entries, int count);
int checkout_validate_entries_available(checkout_entry **entries, int count);
int checkout_restore_state(checkout_entry **current_entries, int current_count,
    checkout_entry **target_entries, int target_count,
    checkout_entry **surviving_entries, int surviving_count);
int checkout_target_conflicts_with_untracked(checkout_entry **target_entries,
    int target_count, checkout_entry **current_entries, int current_count);

#endif
