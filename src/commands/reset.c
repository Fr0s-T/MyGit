#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "checkout_apply.h"
#include "checkout_index.h"
#include "checkout_prepare.h"
#include "checkout_tree.h"
#include "helpers/file_io.h"
#include "node.h"
#include "reset.h"

static int input_check(int argc, char **argv, char **target_commit_hash);

int reset_cmd(int argc, char **argv) {
    char *target_commit_hash;
    char *current_ref_path;
    char *current_commit_hash;
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

    target_commit_hash = NULL;
    current_ref_path = NULL;
    current_commit_hash = NULL;
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
    if (input_check(argc, argv, &target_commit_hash) != 0) {
        return (-1);
    }
    if (file_io_read_first_line(".mygit/HEAD", &current_ref_path) != 0) {
        printf("[reset] failed to read current HEAD\n");
        goto cleanup;
    }
    if (current_ref_path[0] == '\0') {
        printf("[reset] current HEAD is empty\n");
        goto cleanup;
    }
    if (file_io_read_first_line(current_ref_path, &current_commit_hash) != 0) {
        printf("[reset] failed to read current branch ref\n");
        goto cleanup;
    }
    if (checkout_repo_is_up_to_date_with_branch() != 0) {
        printf("[reset] add and commit then try to reset\n");
        goto cleanup;
    }
    if (checkout_read_target_root(target_commit_hash,
            &target_root_hash, &target_root_path) != 0) {
        printf("[reset] failed to read target commit '%s'\n", target_commit_hash);
        goto cleanup;
    }
    if (checkout_collect_current_tracked_entries(&current_entries,
            &current_entry_count) != 0) {
        printf("[reset] failed to read current tracked entries\n");
        goto cleanup;
    }
    root = node_create("", target_root_hash, NODE_ROOT, NULL);
    if (root == NULL) {
        printf("[reset] failed to create root node\n");
        goto cleanup;
    }
    if (target_root_path != NULL) {
        if (checkout_generate_tree(root, target_root_path, &target_entries,
                &target_entry_count) != 0) {
            printf("[reset] failed to build target tree\n");
            goto cleanup;
        }
    }
    if (checkout_build_surviving_entries(&surviving_entries, target_entries,
            current_entries, target_entry_count, current_entry_count,
            &surviving_count) != 0) {
        printf("[reset] failed to build surviving entries\n");
        goto cleanup;
    }
    if (checkout_validate_entries_available(current_entries,
            current_entry_count) != 0) {
        printf("[reset] current tracked objects are incomplete\n");
        goto cleanup;
    }
    if (checkout_validate_entries_available(target_entries,
            target_entry_count) != 0) {
        printf("[reset] target tracked objects are incomplete\n");
        goto cleanup;
    }
    conflict_status = checkout_target_conflicts_with_untracked(target_entries,
        target_entry_count, current_entries, current_entry_count);
    if (conflict_status < 0) {
        printf("[reset] failed to check untracked conflicts\n");
        goto cleanup;
    }
    if (conflict_status > 0) {
        printf("[reset] target commit would overwrite untracked files\n");
        goto cleanup;
    }
    if (checkout_purge_non_surviving_current_entries(current_entries,
            current_entry_count, surviving_entries, surviving_count) != 0) {
        printf("[reset] failed to purge current tracked entries\n");
        if (checkout_restore_state(current_entries, current_entry_count,
                target_entries, target_entry_count, surviving_entries,
                surviving_count) != 0) {
            printf("[reset] rollback failed after purge\n");
        }
        goto cleanup;
    }
    if (checkout_materialize_target_entries(target_entries,
            target_entry_count) != 0) {
        printf("[reset] failed to materialize target entries\n");
        if (checkout_restore_state(current_entries, current_entry_count,
                target_entries, target_entry_count, surviving_entries,
                surviving_count) != 0) {
            printf("[reset] rollback failed after materialize\n");
        }
        goto cleanup;
    }
    if (checkout_rewrite_index_from_entries(target_entries,
            target_entry_count) != 0) {
        printf("[reset] failed to rewrite index\n");
        if (checkout_restore_state(current_entries, current_entry_count,
                target_entries, target_entry_count, surviving_entries,
                surviving_count) != 0) {
            printf("[reset] rollback failed after index rewrite\n");
        }
        goto cleanup;
    }
    if (file_io_write_text(current_ref_path, target_commit_hash) != 0) {
        printf("[reset] failed to update current branch ref\n");
        if (checkout_restore_state(current_entries, current_entry_count,
                target_entries, target_entry_count, surviving_entries,
                surviving_count) != 0) {
            printf("[reset] rollback failed after ref update\n");
        }
        if (current_commit_hash != NULL
                && file_io_write_text(current_ref_path, current_commit_hash) != 0) {
            printf("[reset] failed to restore current branch ref\n");
        }
        goto cleanup;
    }
    printf("[reset] moved current branch to '%s'\n", target_commit_hash);
    status = 0;

cleanup:
    checkout_destroy_entries(current_entries, current_entry_count);
    checkout_destroy_entries(target_entries, target_entry_count);
    checkout_destroy_entries(surviving_entries, surviving_count);
    node_destroy(root);
    free(target_root_path);
    free(target_root_hash);
    free(current_commit_hash);
    free(current_ref_path);
    return (status);
}

static int input_check(int argc, char **argv, char **target_commit_hash) {
    if (target_commit_hash == NULL) {
        return (-1);
    }
    if (argc != 4 || strcmp(argv[1], "reset") != 0
            || strcmp(argv[2], "-r") != 0) {
        printf("[reset] usage: mygit reset -r commit_hash\n");
        return (-1);
    }
    if (argv[3] == NULL || argv[3][0] == '\0') {
        printf("[reset] commit hash is empty\n");
        return (-1);
    }
    *target_commit_hash = argv[3];
    return (0);
}
