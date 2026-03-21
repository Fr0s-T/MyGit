#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <linux/limits.h>

#include "add_creating_blob_and_indexing.h"
#include "hash.h"
#include "helpers/commit_object.h"
#include "helpers/file_io.h"
#include "services.h"

static char *duplicate_string(const char *value);
static int write_object_file(const char *object_hash, const char *payload);
static int build_commit_payload(const char *root_hash, const char *branch_name,
    const char *parent_hash, const char *commit_msg, char **payload);
static char *extract_branch_name(const char *ref_path);

int accept_commit(node *root, const char *commit_msg,
    char out[SHA1_HEX_BUFFER_SIZE]) {
    char *head_ref_path;
    char *current_commit_hash;
    char *previous_tree_hash;
    char *branch_name;
    char *payload;
    char *new_commit_hash;
    int status;

    head_ref_path = NULL;
    current_commit_hash = NULL;
    previous_tree_hash = NULL;
    branch_name = NULL;
    payload = NULL;
    new_commit_hash = NULL;
    status = -1;
    if (root == NULL || root->hash == NULL || commit_msg == NULL || out == NULL) {
        return (-1);
    }
    if (file_io_read_first_line(".mygit/HEAD", &head_ref_path) == -1) {
        goto cleanup;
    }
    if (head_ref_path[0] == '\0') {
        goto cleanup;
    }
    if (file_io_read_first_line(head_ref_path, &current_commit_hash) == -1) {
        goto cleanup;
    }
    if (current_commit_hash[0] != '\0') {
        if (read_commit_tree_hash(current_commit_hash, &previous_tree_hash) == -1) {
            goto cleanup;
        }
        if (strcmp(previous_tree_hash, root->hash) == 0) {
            status = COMMIT_NOTHING_TO_DO;
            goto cleanup;
        }
    }
    branch_name = extract_branch_name(head_ref_path);
    if (branch_name == NULL) {
        goto cleanup;
    }
    if (build_commit_payload(root->hash, branch_name, current_commit_hash,
            commit_msg, &payload) == -1) {
        goto cleanup;
    }
    new_commit_hash = sha1(payload);
    if (new_commit_hash == NULL) {
        goto cleanup;
    }
    if (write_object_file(new_commit_hash, payload) == -1) {
        goto cleanup;
    }
    if (file_io_write_text(head_ref_path, new_commit_hash) == -1) {
        goto cleanup;
    }
    strcpy(out, new_commit_hash);
    status = 0;

cleanup:
    free(new_commit_hash);
    free(payload);
    free(branch_name);
    free(previous_tree_hash);
    free(current_commit_hash);
    free(head_ref_path);
    return (status);
}

static char *duplicate_string(const char *value) {
    char *copy;

    if (value == NULL) {
        return (NULL);
    }
    copy = malloc(strlen(value) + 1);
    if (copy == NULL) {
        return (NULL);
    }
    strcpy(copy, value);
    return (copy);
}

static int write_object_file(const char *object_hash, const char *payload) {
    char cwd[PATH_MAX];
    char *git_obj_path;
    char *object_path;
    int status;

    if (object_hash == NULL || payload == NULL) {
        return (-1);
    }
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        return (-1);
    }
    git_obj_path = create_git_obj_dir(cwd);
    if (git_obj_path == NULL) {
        return (-1);
    }
    object_path = generate_path(git_obj_path, object_hash);
    free(git_obj_path);
    if (object_path == NULL) {
        return (-1);
    }
    status = file_io_write_text(object_path, payload);
    if (status == -1) {
        remove(object_path);
    }
    free(object_path);
    return (status);
}

int read_commit_tree_hash(const char *commit_hash, char **tree_hash) {
    char cwd[PATH_MAX];
    char *git_obj_path;
    char *commit_object_path;
    char *first_line;

    if (commit_hash == NULL || tree_hash == NULL) {
        return (-1);
    }
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        return (-1);
    }
    git_obj_path = create_git_obj_dir(cwd);
    if (git_obj_path == NULL) {
        return (-1);
    }
    commit_object_path = generate_path(git_obj_path, commit_hash);
    free(git_obj_path);
    if (commit_object_path == NULL) {
        return (-1);
    }
    if (file_io_read_first_line(commit_object_path, &first_line) == -1) {
        free(commit_object_path);
        return (-1);
    }
    free(commit_object_path);
    if (strncmp(first_line, "tree ", 5) != 0) {
        free(first_line);
        return (-1);
    }
    *tree_hash = duplicate_string(first_line + 5);
    free(first_line);
    if (*tree_hash == NULL) {
        return (-1);
    }
    return (0);
}

static int build_commit_payload(const char *root_hash, const char *branch_name,
    const char *parent_hash, const char *commit_msg, char **payload) {
    time_t now;
    char time_buffer[32];
    int needed;
    const char *parent_value;

    if (root_hash == NULL || branch_name == NULL || commit_msg == NULL || payload == NULL) {
        return (-1);
    }
    now = time(NULL);
    if (now == (time_t)-1) {
        return (-1);
    }
    snprintf(time_buffer, sizeof(time_buffer), "%ld", (long)now);
    parent_value = (parent_hash != NULL && parent_hash[0] != '\0') ? parent_hash : "NULL";
    needed = snprintf(NULL, 0,
        "tree %s\nbranch %s\ntime %s\nparent %s\n\n%s",
        root_hash, branch_name, time_buffer, parent_value, commit_msg);
    if (needed < 0) {
        return (-1);
    }
    *payload = malloc((size_t)needed + 1);
    if (*payload == NULL) {
        return (-1);
    }
    snprintf(*payload, (size_t)needed + 1,
        "tree %s\nbranch %s\ntime %s\nparent %s\n\n%s",
        root_hash, branch_name, time_buffer, parent_value, commit_msg);
    return (0);
}

static char *extract_branch_name(const char *ref_path) {
    const char *last_slash;

    if (ref_path == NULL) {
        return (NULL);
    }
    last_slash = strrchr(ref_path, '/');
    if (last_slash == NULL || *(last_slash + 1) == '\0') {
        return (duplicate_string(ref_path));
    }
    return (duplicate_string(last_slash + 1));
}
