#ifndef MYGIT_MERGE_PREPARE_H
#define MYGIT_MERGE_PREPARE_H

#include "checkout_entry.h"

typedef enum e_merge_mode {
    MERGE_MODE_NONE,
    MERGE_MODE_INCOMING,
    MERGE_MODE_CURRENT
} merge_mode;

int merge_input_check(int argc, char **argv, merge_mode *mode,
    char **branch_name);
int merge_read_current_state(char **current_ref_path, char **current_commit_hash);
int merge_prompt_use_fast_forward(const char *branch_name, merge_mode mode);
int merge_collect_entries_from_commit(const char *commit_hash,
    checkout_entry ***entries, int *entry_count);
int merge_commit_is_ancestor(const char *ancestor_hash, const char *commit_hash);
int merge_find_merge_base(const char *left_hash, const char *right_hash,
    char **merge_base_hash);
char *merge_build_default_message(const char *branch_name);

#endif
