#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "branch.h"
#include "checkout_apply.h"
#include "checkout_index.h"
#include "checkout_prepare.h"
#include "helpers/commit_object.h"
#include "helpers/commit_tree.h"
#include "helpers/file_io.h"
#include "merge.h"
#include "merge_engine.h"
#include "merge_prepare.h"

static int merge_apply_entries(checkout_entry **current_entries, int current_count,
    checkout_entry **target_entries, int target_count,
    checkout_entry ***surviving_entries, int *surviving_count);
static int merge_fast_forward_to_target(const char *current_ref_path,
    const char *current_commit_hash, const char *target_commit_hash);
static void merge_print_conflicts(const char *branch_name, char **conflict_paths,
    int conflict_count);

int merge_cmd(int argc, char **argv) {
    char *branch_name;
    merge_mode mode;
    char *current_ref_path;
    char *current_commit_hash;
    char *target_ref_path;
    char *target_commit_hash;
    char *merge_base_hash;
    char *merge_message;
    checkout_entry **current_entries;
    checkout_entry **target_entries;
    checkout_entry **base_entries;
    checkout_entry **merged_entries;
    checkout_entry **surviving_entries;
    char **conflict_paths;
    node *root;
    char *parent_hashes[2];
    char merge_commit_hash[SHA1_HEX_BUFFER_SIZE];
    int current_count;
    int target_count;
    int base_count;
    int merged_count;
    int surviving_count;
    int fast_forward_possible;
    int prompt_status;
    int target_is_ancestor;
    int conflict_count;
    int status;

    branch_name = NULL;
    current_ref_path = NULL;
    current_commit_hash = NULL;
    target_ref_path = NULL;
    target_commit_hash = NULL;
    merge_base_hash = NULL;
    merge_message = NULL;
    current_entries = NULL;
    target_entries = NULL;
    base_entries = NULL;
    merged_entries = NULL;
    surviving_entries = NULL;
    conflict_paths = NULL;
    root = NULL;
    current_count = 0;
    target_count = 0;
    base_count = 0;
    merged_count = 0;
    surviving_count = 0;
    fast_forward_possible = 0;
    prompt_status = 0;
    target_is_ancestor = 0;
    conflict_count = 0;
    status = -1;
    if (merge_input_check(argc, argv, &mode, &branch_name) != 0) {
        return (-1);
    }
    if (branch_name == NULL || branch_name[0] == '\0') {
        printf("[merge] branch name is empty\n");
        return (-1);
    }
    if (checkout_branch_exists(branch_name) == 0) {
        printf("[merge] branch '%s' does not exist\n", branch_name);
        return (-1);
    }
    if (checkout_repo_is_up_to_date_with_branch() != 0) {
        printf("[merge] add and commit then try to merge\n");
        return (-1);
    }
    if (merge_read_current_state(&current_ref_path, &current_commit_hash) != 0) {
        printf("[merge] failed to read current branch state\n");
        goto cleanup;
    }
    if (checkout_read_target_commit(branch_name, &target_ref_path,
            &target_commit_hash) != 0) {
        printf("[merge] failed to read target branch '%s'\n", branch_name);
        goto cleanup;
    }
    if (target_commit_hash[0] == '\0') {
        printf("[merge] target branch '%s' has no commits\n", branch_name);
        status = 0;
        goto cleanup;
    }
    if ((current_ref_path != NULL && target_ref_path != NULL
            && strcmp(current_ref_path, target_ref_path) == 0)
            || (current_commit_hash != NULL && target_commit_hash != NULL
                && strcmp(current_commit_hash, target_commit_hash) == 0)) {
        printf("[merge] already up to date\n");
        status = 0;
        goto cleanup;
    }
    if (current_commit_hash[0] == '\0') {
        fast_forward_possible = 1;
    }
    else {
        fast_forward_possible = merge_commit_is_ancestor(current_commit_hash,
            target_commit_hash);
        if (fast_forward_possible < 0) {
            printf("[merge] failed to inspect fast-forward history\n");
            goto cleanup;
        }
    }
    if (mode == MERGE_MODE_NONE && current_commit_hash[0] != '\0') {
        target_is_ancestor = merge_commit_is_ancestor(target_commit_hash,
            current_commit_hash);
        if (target_is_ancestor < 0) {
            printf("[merge] failed to inspect target ancestry\n");
            goto cleanup;
        }
        if (target_is_ancestor > 0) {
            printf("[merge] already up to date\n");
            status = 0;
            goto cleanup;
        }
    }
    if (fast_forward_possible > 0) {
        if (mode == MERGE_MODE_NONE) {
            if (merge_fast_forward_to_target(current_ref_path,
                    current_commit_hash, target_commit_hash) != 0) {
                goto cleanup;
            }
            printf("[merge] fast-forwarded current branch to '%s'\n", branch_name);
            status = 0;
            goto cleanup;
        }
        prompt_status = merge_prompt_use_fast_forward(branch_name, mode);
        if (prompt_status < 0) {
            printf("[merge] failed to read fast-forward answer\n");
            goto cleanup;
        }
        if (prompt_status > 0) {
            if (merge_fast_forward_to_target(current_ref_path,
                    current_commit_hash, target_commit_hash) != 0) {
                goto cleanup;
            }
            printf("[merge] fast-forwarded current branch to '%s'\n", branch_name);
            status = 0;
            goto cleanup;
        }
        if (current_commit_hash[0] == '\0') {
            printf("[merge] cannot force a non-fast-forward merge from an empty current branch\n");
            goto cleanup;
        }
    }
    if (current_commit_hash[0] == '\0') {
        printf("[merge] current branch has no commits to merge into\n");
        goto cleanup;
    }
    if (merge_find_merge_base(current_commit_hash, target_commit_hash,
            &merge_base_hash) != 0) {
        printf("[merge] failed to find merge base\n");
        goto cleanup;
    }
    if (checkout_collect_current_tracked_entries(&current_entries,
            &current_count) != 0) {
        printf("[merge] failed to read current tracked entries\n");
        goto cleanup;
    }
    if (merge_collect_entries_from_commit(target_commit_hash, &target_entries,
            &target_count) != 0) {
        printf("[merge] failed to read target tracked entries\n");
        goto cleanup;
    }
    if (merge_collect_entries_from_commit(merge_base_hash, &base_entries,
            &base_count) != 0) {
        printf("[merge] failed to read merge-base tracked entries\n");
        goto cleanup;
    }
    if (merge_build_entries(base_entries, base_count, current_entries,
            current_count, target_entries, target_count, mode, &merged_entries,
            &merged_count, &conflict_paths, &conflict_count) != 0) {
        printf("[merge] failed to build merged entries\n");
        goto cleanup;
    }
    if (mode == MERGE_MODE_NONE && conflict_count > 0) {
        merge_print_conflicts(branch_name, conflict_paths, conflict_count);
        goto cleanup;
    }
    merge_message = merge_build_default_message(branch_name);
    if (merge_message == NULL) {
        printf("[merge] failed to build merge message\n");
        goto cleanup;
    }
    if (merge_apply_entries(current_entries, current_count, merged_entries,
            merged_count, &surviving_entries, &surviving_count) != 0) {
        goto cleanup;
    }
    if (build_tree_from_index_path(".mygit/index", 1, &root) != 0) {
        printf("[merge] failed to build merge tree\n");
        if (checkout_restore_state(current_entries, current_count, merged_entries,
                merged_count, surviving_entries, surviving_count) != 0) {
            printf("[merge] rollback failed after merge tree build\n");
        }
        goto cleanup;
    }
    parent_hashes[0] = current_commit_hash;
    parent_hashes[1] = target_commit_hash;
    if (accept_commit_with_parents(root, merge_message, parent_hashes, 2,
            merge_commit_hash) != 0) {
        printf("[merge] failed to write merge commit\n");
        if (checkout_restore_state(current_entries, current_count, merged_entries,
                merged_count, surviving_entries, surviving_count) != 0) {
            printf("[merge] rollback failed after merge commit\n");
        }
        goto cleanup;
    }
    printf("[merge] merged '%s' into current branch\n", branch_name);
    printf("[merge] commit: %s\n", merge_commit_hash);
    status = 0;

cleanup:
    checkout_destroy_entries(current_entries, current_count);
    checkout_destroy_entries(target_entries, target_count);
    checkout_destroy_entries(base_entries, base_count);
    checkout_destroy_entries(merged_entries, merged_count);
    checkout_destroy_entries(surviving_entries, surviving_count);
    merge_destroy_conflicts(conflict_paths, conflict_count);
    node_destroy(root);
    free(merge_message);
    free(merge_base_hash);
    free(target_commit_hash);
    free(target_ref_path);
    free(current_commit_hash);
    free(current_ref_path);
    return (status);
}

static int merge_apply_entries(checkout_entry **current_entries, int current_count,
    checkout_entry **target_entries, int target_count,
    checkout_entry ***surviving_entries, int *surviving_count) {
    int conflict_status;

    conflict_status = 0;
    if (surviving_entries == NULL || surviving_count == NULL) {
        return (-1);
    }
    *surviving_entries = NULL;
    *surviving_count = 0;
    if (checkout_build_surviving_entries(surviving_entries, target_entries,
            current_entries, target_count, current_count, surviving_count) != 0) {
        printf("[merge] failed to build surviving entries\n");
        return (-1);
    }
    if (checkout_validate_entries_available(current_entries, current_count) != 0) {
        printf("[merge] current tracked objects are incomplete\n");
        goto cleanup;
    }
    if (checkout_validate_entries_available(target_entries, target_count) != 0) {
        printf("[merge] merged tracked objects are incomplete\n");
        goto cleanup;
    }
    conflict_status = checkout_target_conflicts_with_untracked(target_entries,
        target_count, current_entries, current_count);
    if (conflict_status < 0) {
        printf("[merge] failed to check untracked conflicts\n");
        goto cleanup;
    }
    if (conflict_status > 0) {
        printf("[merge] merge result would overwrite untracked files\n");
        goto cleanup;
    }
    if (checkout_purge_non_surviving_current_entries(current_entries,
            current_count, *surviving_entries, *surviving_count) != 0) {
        printf("[merge] failed to purge current tracked entries\n");
        if (checkout_restore_state(current_entries, current_count, target_entries,
                target_count, *surviving_entries, *surviving_count) != 0) {
            printf("[merge] rollback failed after purge\n");
        }
        goto cleanup;
    }
    if (checkout_materialize_target_entries(target_entries, target_count) != 0) {
        printf("[merge] failed to materialize merged entries\n");
        if (checkout_restore_state(current_entries, current_count, target_entries,
                target_count, *surviving_entries, *surviving_count) != 0) {
            printf("[merge] rollback failed after materialize\n");
        }
        goto cleanup;
    }
    if (checkout_rewrite_index_from_entries(target_entries, target_count) != 0) {
        printf("[merge] failed to rewrite index\n");
        if (checkout_restore_state(current_entries, current_count, target_entries,
                target_count, *surviving_entries, *surviving_count) != 0) {
            printf("[merge] rollback failed after index rewrite\n");
        }
        goto cleanup;
    }
    return (0);

cleanup:
    checkout_destroy_entries(*surviving_entries, *surviving_count);
    *surviving_entries = NULL;
    *surviving_count = 0;
    return (-1);
}

static int merge_fast_forward_to_target(const char *current_ref_path,
    const char *current_commit_hash, const char *target_commit_hash) {
    checkout_entry **current_entries;
    checkout_entry **target_entries;
    checkout_entry **surviving_entries;
    int current_count;
    int target_count;
    int surviving_count;
    int status;

    current_entries = NULL;
    target_entries = NULL;
    surviving_entries = NULL;
    current_count = 0;
    target_count = 0;
    surviving_count = 0;
    status = -1;
    if (current_ref_path == NULL || current_commit_hash == NULL
            || target_commit_hash == NULL) {
        return (-1);
    }
    if (checkout_collect_current_tracked_entries(&current_entries,
            &current_count) != 0) {
        printf("[merge] failed to read current tracked entries\n");
        goto cleanup;
    }
    if (merge_collect_entries_from_commit(target_commit_hash, &target_entries,
            &target_count) != 0) {
        printf("[merge] failed to read fast-forward target entries\n");
        goto cleanup;
    }
    if (merge_apply_entries(current_entries, current_count, target_entries,
            target_count, &surviving_entries, &surviving_count) != 0) {
        goto cleanup;
    }
    if (file_io_write_text(current_ref_path, target_commit_hash) != 0) {
        printf("[merge] failed to update current branch ref\n");
        if (checkout_restore_state(current_entries, current_count, target_entries,
                target_count, surviving_entries, surviving_count) != 0) {
            printf("[merge] rollback failed after ref update\n");
        }
        if (file_io_write_text(current_ref_path, current_commit_hash) != 0) {
            printf("[merge] failed to restore current branch ref\n");
        }
        goto cleanup;
    }
    status = 0;

cleanup:
    checkout_destroy_entries(current_entries, current_count);
    checkout_destroy_entries(target_entries, target_count);
    checkout_destroy_entries(surviving_entries, surviving_count);
    return (status);
}

static void merge_print_conflicts(const char *branch_name, char **conflict_paths,
    int conflict_count) {
    printf("[merge] merge conflicts found with '%s'\n", branch_name);
    for (int i = 0; i < conflict_count; i++) {
        if (conflict_paths == NULL || conflict_paths[i] == NULL) {
            continue;
        }
        printf("[merge] conflict: %s\n", conflict_paths[i]);
    }
}
