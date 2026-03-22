#include "my_includes.h"
#include "checkout_prepare.h"
#include "helpers/commit_object.h"
#include "helpers/commit_tree.h"

static void destroy_checkout_entries(checkout_entry **entries, int count);
static int checkout_all_tracked_files_exist(const char *cwd);
static int checkout_has_modified_tracked_files(const char *cwd);

int checkout_input_check(int argc, char **argv, char **branch_name) {
    if (branch_name == NULL) {
        return (-1);
    }
    if (argc != 3 || strcmp(argv[1], "checkout") != 0) {
        printf("[checkout] usage: mygit checkout branch_name\n");
        return (-1);
    }
    *branch_name = argv[2];
    return (0);
}

int checkout_branch_exists(const char *branch_name) {
    char **names;
    int count;
    int exists;

    names = NULL;
    count = 0;
    exists = 0;
    if (branch_name == NULL) {
        return (0);
    }
    if (branch_load_names(&names, &count) != 0) {
        return (0);
    }
    for (int i = 0; i < count; i++) {
        if (strcmp(names[i], branch_name) == 0) {
            exists = 1;
            break;
        }
    }
    branch_destroy_names(names, count);
    return (exists);
}

int checkout_repo_is_up_to_date_with_branch(void) {
    char cwd[PATH_MAX];
    char *head_ref_path;
    char *head_commit_hash;
    char *head_tree_hash;
    int tracked_change_status;
    char *git_dir_path;
    char *index_path;
    node *root;
    int status;

    head_ref_path = NULL;
    head_commit_hash = NULL;
    head_tree_hash = NULL;
    tracked_change_status = -1;
    git_dir_path = NULL;
    index_path = NULL;
    root = NULL;
    status = -1;

    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        goto cleanup;
    }
    tracked_change_status = checkout_has_modified_tracked_files(cwd);
    if (tracked_change_status < 0) {
        goto cleanup;
    }
    if (tracked_change_status > 0) {
        goto cleanup;
    }
    if (checkout_all_tracked_files_exist(cwd) != 0) {
        goto cleanup;
    }
    if (file_io_read_first_line(".mygit/HEAD", &head_ref_path) != 0) {
        goto cleanup;
    }
    if (file_io_read_first_line(head_ref_path, &head_commit_hash) != 0) {
        goto cleanup;
    }
    if (head_commit_hash[0] == '\0') {
        status = 0;
        goto cleanup;
    }
    if (read_commit_tree_hash(head_commit_hash, &head_tree_hash) != 0) {
        goto cleanup;
    }
    git_dir_path = generate_path(cwd, ".mygit");
    if (git_dir_path == NULL) {
        goto cleanup;
    }
    index_path = generate_path(git_dir_path, "index");
    if (index_path == NULL) {
        goto cleanup;
    }
    if (build_tree_from_index_path(index_path, 0, &root) != 0) {
        goto cleanup;
    }
    if (root->hash == NULL) {
        goto cleanup;
    }
    if (strcmp(root->hash, head_tree_hash) != 0) {
        goto cleanup;
    }
    status = 0;

cleanup:
    node_destroy(root);
    free(index_path);
    free(git_dir_path);
    free(head_tree_hash);
    free(head_commit_hash);
    free(head_ref_path);
    return (status);
}

int checkout_read_target_commit(const char *branch_name,
    char **target_ref_path, char **target_commit_hash) {
    int needed;

    if (branch_name == NULL || target_ref_path == NULL || target_commit_hash == NULL) {
        return (-1);
    }
    *target_ref_path = NULL;
    *target_commit_hash = NULL;
    needed = snprintf(NULL, 0, ".mygit/refs/heads/%s", branch_name);
    if (needed < 0) {
        return (-1);
    }
    *target_ref_path = malloc((size_t)needed + 1);
    if (*target_ref_path == NULL) {
        return (-1);
    }
    snprintf(*target_ref_path, (size_t)needed + 1, ".mygit/refs/heads/%s", branch_name);
    if (file_io_read_first_line(*target_ref_path, target_commit_hash) != 0) {
        free(*target_ref_path);
        *target_ref_path = NULL;
        return (-1);
    }
    if (*target_commit_hash == NULL) {
        free(*target_ref_path);
        *target_ref_path = NULL;
        return (-1);
    }
    return (0);
}

int checkout_read_target_root(const char *target_commit_hash,
    char **root_hash, char **root_path) {
    char *commit_path;
    int needed;
    int strip_status;

    commit_path = NULL;
    if (target_commit_hash == NULL || root_hash == NULL || root_path == NULL) {
        return (-1);
    }
    *root_hash = NULL;
    *root_path = NULL;
    if (target_commit_hash[0] == '\0') {
        return (0);
    }
    needed = snprintf(NULL, 0, ".mygit/objects/%s", target_commit_hash);
    if (needed < 0) {
        return (-1);
    }
    commit_path = malloc((size_t)needed + 1);
    if (commit_path == NULL) {
        return (-1);
    }
    snprintf(commit_path, (size_t)needed + 1, ".mygit/objects/%s", target_commit_hash);
    if (file_io_read_first_line(commit_path, root_hash) != 0) {
        free(commit_path);
        return (-1);
    }
    free(commit_path);
    strip_status = file_io_strip_substring(*root_hash, "tree ");
    if (strip_status != 1) {
        free(*root_hash);
        *root_hash = NULL;
        return (-1);
    }
    needed = snprintf(NULL, 0, ".mygit/objects/%s", *root_hash);
    if (needed < 0) {
        free(*root_hash);
        *root_hash = NULL;
        return (-1);
    }
    *root_path = malloc((size_t)needed + 1);
    if (*root_path == NULL) {
        free(*root_hash);
        *root_hash = NULL;
        return (-1);
    }
    snprintf(*root_path, (size_t)needed + 1, ".mygit/objects/%s", *root_hash);
    return (0);
}

int checkout_update_head_ref(const char *target_ref_path) {
    if (target_ref_path == NULL) {
        return (-1);
    }
    return (file_io_write_text(".mygit/HEAD", target_ref_path));
}

static void destroy_checkout_entries(checkout_entry **entries, int count) {
    if (entries == NULL) {
        return;
    }
    for (int i = 0; i < count; i++) {
        checkout_entry_destroy(entries[i]);
    }
    free(entries);
}

static int checkout_all_tracked_files_exist(const char *cwd) {
    checkout_entry **current_entries;
    int current_count;
    int status;

    current_entries = NULL;
    current_count = 0;
    status = -1;
    if (cwd == NULL) {
        return (-1);
    }
    if (checkout_collect_current_tracked_entries(&current_entries,
            &current_count) != 0) {
        goto cleanup;
    }
    for (int i = 0; i < current_count; i++) {
        char *full_path;

        if (current_entries[i] == NULL || current_entries[i]->relative_path == NULL) {
            goto cleanup;
        }
        full_path = generate_path(cwd, current_entries[i]->relative_path);
        if (full_path == NULL) {
            goto cleanup;
        }
        if (access(full_path, F_OK) != 0) {
            free(full_path);
            goto cleanup;
        }
        free(full_path);
    }
    status = 0;

cleanup:
    destroy_checkout_entries(current_entries, current_count);
    return (status);
}

static int checkout_has_modified_tracked_files(const char *cwd) {
    file_data **changed_files;
    int changed_count;
    char index_path[PATH_MAX];
    int status;

    changed_files = NULL;
    changed_count = 0;
    status = -1;
    if (cwd == NULL) {
        return (-1);
    }
    if (snprintf(index_path, sizeof(index_path), "%s/.mygit/index", cwd)
            >= (int)sizeof(index_path)) {
        return (-1);
    }
    if (add_collect_changed_files(cwd, &changed_files, &changed_count) != 0) {
        goto cleanup;
    }
    for (int i = 0; i < changed_count; i++) {
        char tracked_hash[SHA1_HEX_BUFFER_SIZE];
        int lookup_status;

        if (changed_files[i] == NULL || changed_files[i]->path == NULL) {
            goto cleanup;
        }
        lookup_status = file_io_read_index_hash(index_path, changed_files[i]->path,
            tracked_hash);
        if (lookup_status < 0) {
            goto cleanup;
        }
        if (lookup_status == 1) {
            status = 1;
            goto cleanup;
        }
    }
    status = 0;

cleanup:
    add_destroy_file_list(changed_files, changed_count);
    return (status);
}
