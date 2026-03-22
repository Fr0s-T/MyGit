#include "my_includes.h"

#include "checkout_apply.h"
#include "checkout_prepare.h"
#include "checkout_index.h"

int checkout(int argc, char **argv) {
    char *branch_name;
    char *current_ref_path;
    char *target_ref_path;
    char *target_commit_hash;
    char *target_root_hash;
    char *target_root_path;
    checkout_entry **current_entries;
    checkout_entry **target_entries;
    checkout_entry **surviving_entries;
    node *root;
    int current_entry_count;
    int target_entry_count;
    int surviving_count;
    int conflict_status;
    int status;

    branch_name = NULL;
    current_ref_path = NULL;
    target_ref_path = NULL;
    target_commit_hash = NULL;
    target_root_hash = NULL;
    target_root_path = NULL;
    current_entries = NULL;
    target_entries = NULL;
    surviving_entries = NULL;
    root = NULL;
    current_entry_count = 0;
    target_entry_count = 0;
    surviving_count = 0;
    conflict_status = 0;
    status = -1;
    if (checkout_input_check(argc, argv, &branch_name) != 0) {
        return (-1);
    }
    if (checkout_branch_exists(branch_name) == 0) {
        printf("[checkout] branch '%s' does not exist\n", branch_name);
        return (-1);
    }
    if (file_io_read_first_line(".mygit/HEAD", &current_ref_path) != 0) {
        printf("[checkout] failed to read current HEAD\n");
        return (-1);
    }
    if (checkout_repo_is_up_to_date_with_branch() != 0) {
        printf("[checkout] add and commit then try to checkout\n");
        goto cleanup;
    }
    if (checkout_read_target_commit(branch_name,
            &target_ref_path, &target_commit_hash) != 0) {
        printf("[checkout] failed to read target branch '%s'\n", branch_name);
        goto cleanup;
    }
    if (checkout_read_target_root(target_commit_hash,
            &target_root_hash, &target_root_path) != 0) {
        printf("[checkout] failed to read target root for '%s'\n", branch_name);
        goto cleanup;
    }
    if (checkout_collect_current_tracked_entries(&current_entries,
            &current_entry_count) != 0) {
        printf("[checkout] failed to read current tracked entries\n");
        goto cleanup;
    }
    root = node_create("", target_root_hash, NODE_ROOT, NULL);
    if (root == NULL) {
        printf("[checkout] failed to create root node\n");
        goto cleanup;
    }
    if (target_root_path != NULL) {
        if (checkout_generate_tree(root, target_root_path, &target_entries,
                &target_entry_count) != 0) {
            printf("[checkout] failed to build target tree\n");
            goto cleanup;
        }
    }
    if (checkout_build_surviving_entries(&surviving_entries, target_entries,
            current_entries, target_entry_count, current_entry_count,
            &surviving_count) != 0) {
        printf("[checkout] failed to build surviving entries\n");
        goto cleanup;
    }
    if (checkout_validate_entries_available(current_entries,
            current_entry_count) != 0) {
        printf("[checkout] current tracked objects are incomplete\n");
        goto cleanup;
    }
    if (checkout_validate_entries_available(target_entries,
            target_entry_count) != 0) {
        printf("[checkout] target tracked objects are incomplete\n");
        goto cleanup;
    }
    conflict_status = checkout_target_conflicts_with_untracked(target_entries,
        target_entry_count, current_entries, current_entry_count);
    if (conflict_status < 0) {
        printf("[checkout] failed to check untracked conflicts\n");
        goto cleanup;
    }
    if (conflict_status > 0) {
        printf("[checkout] target branch would overwrite untracked files\n");
        goto cleanup;
    }
    if (checkout_purge_non_surviving_current_entries(current_entries,
            current_entry_count, surviving_entries, surviving_count) != 0) {
        printf("[checkout] failed to purge current tracked entries\n");
        if (checkout_restore_state(current_entries, current_entry_count,
                target_entries, target_entry_count, surviving_entries,
                surviving_count) != 0) {
            printf("[checkout] rollback failed after purge\n");
        }
        goto cleanup;
    }
    if (checkout_materialize_target_entries(target_entries,
            target_entry_count) != 0) {
        printf("[checkout] failed to materialize target entries\n");
        if (checkout_restore_state(current_entries, current_entry_count,
                target_entries, target_entry_count, surviving_entries,
                surviving_count) != 0) {
            printf("[checkout] rollback failed after materialize\n");
        }
        goto cleanup;
    }
    if (checkout_rewrite_index_from_entries(target_entries,
            target_entry_count) != 0) {
        printf("[checkout] failed to rewrite index\n");
        if (checkout_restore_state(current_entries, current_entry_count,
                target_entries, target_entry_count, surviving_entries,
                surviving_count) != 0) {
            printf("[checkout] rollback failed after index rewrite\n");
        }
        goto cleanup;
    }
    if (checkout_update_head_ref(target_ref_path) != 0) {
        printf("[checkout] failed to update HEAD\n");
        if (checkout_restore_state(current_entries, current_entry_count,
                target_entries, target_entry_count, surviving_entries,
                surviving_count) != 0) {
            printf("[checkout] rollback failed after HEAD update\n");
        }
        if (current_ref_path != NULL
                && checkout_update_head_ref(current_ref_path) != 0) {
            printf("[checkout] failed to restore HEAD\n");
        }
        goto cleanup;
    }
    printf("[checkout] switched to branch '%s'\n", branch_name);
    status = 0;

cleanup:
    checkout_destroy_entries(current_entries, current_entry_count);
    checkout_destroy_entries(target_entries, target_entry_count);
    checkout_destroy_entries(surviving_entries, surviving_count);
    node_destroy(root);
    free(target_root_path);
    free(target_root_hash);
    free(target_commit_hash);
    free(target_ref_path);
    free(current_ref_path);
    return (status);
}
