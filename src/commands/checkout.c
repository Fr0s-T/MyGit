#include "my_includes.h"
#include "checkout_prepare.h"

static int generate_tree(node *root, const char *root_path);

int checkout(int argc, char **argv) {
    char *branch_name;
    char *target_ref_path;
    char *target_commit_hash;
    char *target_root_hash;
    char *target_root_path;
    node *root;
    int status;

    branch_name = NULL;
    target_ref_path = NULL;
    target_commit_hash = NULL;
    target_root_hash = NULL;
    target_root_path = NULL;
    root = NULL;
    status = -1;
    if (checkout_input_check(argc, argv, &branch_name) != 0) {
        return (-1);
    }
    if (checkout_branch_exists(branch_name) == 0) {
        printf("[checkout] branch '%s' does not exist\n", branch_name);
        return (-1);
    }
    if (checkout_repo_is_up_to_date_with_branch() != 0) {
        printf("[checkout] add and commit then try to checkout\n");
        return (-1);
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
    printf("[checkout] branch '%s' exists\n", branch_name);
    printf("[checkout] repo is up to date with current branch\n");
    printf("[checkout] target ref: %s\n", target_ref_path);
    printf("[checkout] target commit: %s\n", target_commit_hash);
    printf("[checkout] target root hash: %s\n", target_root_hash);
    printf("[checkout] target root path: %s\n", target_root_path);
    root = node_create("", target_root_hash, NODE_ROOT, NULL);
    if (root == NULL) {
        printf("[checkout] failed to create root node\n");
        goto cleanup;
    }
    if (generate_tree(root, target_root_path) != 0) {
        printf("[checkout] failed to build target tree\n");
        goto cleanup;
    }
    print_tree(root, 0);
    status = 0;

cleanup:
    node_destroy(root);
    free(target_root_path);
    free(target_root_hash);
    free(target_commit_hash);
    free(target_ref_path);
    return (status);
}

static int generate_tree(node *root, const char *root_path) {
    FILE *current_tree_object_file;
    char line[4096];
    int status;

    current_tree_object_file = fopen(root_path, "r");
    if (current_tree_object_file == NULL) {
        return (-1);
    }
    status = 0;

    while (fgets(line, sizeof(line), current_tree_object_file) != NULL) {
        char *copy;
        char *token;
        int pos;
        char *node_name;
        char *node_hash;
        node_type type = NODE_UNKNOWN;
        node *new_node;
        char *new_tree_object_path;

        copy = malloc(strlen(line) + 1);
        if (copy == NULL) {
            status = -1;
            goto cleanup;
        }
        strcpy(copy, line);
        token = strtok(copy, " \n");
        pos = 0;
        node_name = NULL;
        node_hash = NULL;
        new_node = NULL;
        new_tree_object_path = NULL;

        while (token != NULL) {
            if (pos == 0) {
                if (strcmp(token, "blob") == 0) {
                    type = NODE_FILE;
                }
                else if (strcmp(token, "tree") == 0) {
                    type = NODE_DIR;
                }
                else {
                    free(copy);
                    status = -1;
                    goto cleanup;
                }
            }
            else if (pos == 1) {
                node_name = malloc(strlen(token) + 1);
                if (node_name == NULL) {
                    free(copy);
                    status = -1;
                    goto cleanup;
                }
                snprintf(node_name, strlen(token) + 1, "%s", token);
            }
            else if (pos == 2) {
                node_hash = malloc(strlen(token) + 1);
                if (node_hash == NULL) {
                    free(node_name);
                    free(copy);
                    status = -1;
                    goto cleanup;
                }
                snprintf(node_hash, strlen(token) + 1, "%s", token);
            }
            pos++;
            token = strtok(NULL, " \n");
        }
        free(copy);
        if (type == NODE_UNKNOWN || node_name == NULL || node_hash == NULL) {
            free(node_name);
            free(node_hash);
            status = -1;
            goto cleanup;
        }
        new_node = node_create(node_name, node_hash, type, root);
        free(node_name);
        free(node_hash);
        if (new_node == NULL) {
            status = -1;
            goto cleanup;
        }
        if (node_add_child(root, new_node) != 0) {
            node_destroy(new_node);
            status = -1;
            goto cleanup;
        }
        if (type == NODE_DIR) {
            new_tree_object_path = generate_path(".mygit/objects", new_node->hash);
            if (new_tree_object_path == NULL) {
                status = -1;
                goto cleanup;
            }
            if (generate_tree(new_node, new_tree_object_path) != 0) {
                free(new_tree_object_path);
                status = -1;
                goto cleanup;
            }
            free(new_tree_object_path);
        }
    }

cleanup:
    fclose(current_tree_object_file);
    return (status);
}
