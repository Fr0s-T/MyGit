#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "checkout_tree.h"
#include "my_includes.h"

static int generate_tree(node *root, const char *root_path,
    checkout_entry ***target_entries, int *entry_count);

int checkout_generate_tree(node *root, const char *root_path,
    checkout_entry ***target_entries, int *entry_count) {
    if (target_entries == NULL || entry_count == NULL) {
        return (-1);
    }
    *target_entries = NULL;
    *entry_count = 0;
    return (generate_tree(root, root_path, target_entries, entry_count));
}

static int generate_tree(node *root, const char *root_path,
    checkout_entry ***target_entries, int *entry_count) {
    FILE *current_tree_object_file;
    char line[4096];
    int status;

    if (root == NULL || root_path == NULL || target_entries == NULL
            || entry_count == NULL) {
        return (-1);
    }
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
        node_type type;
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
        type = NODE_UNKNOWN;
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
            if (generate_tree(new_node, new_tree_object_path,
                    target_entries, entry_count) != 0) {
                free(new_tree_object_path);
                status = -1;
                goto cleanup;
            }
            free(new_tree_object_path);
        }
        if (type == NODE_FILE) {
            checkout_entry **new_entries;
            char *relative_path;
            checkout_entry *new_entry;

            relative_path = NULL;
            new_entry = NULL;
            new_entries = realloc(*target_entries,
                (size_t)(*entry_count + 1) * sizeof(checkout_entry *));
            if (new_entries == NULL) {
                status = -1;
                goto cleanup;
            }
            *target_entries = new_entries;
            if (get_nodes_path(new_node, &relative_path) != 0) {
                status = -1;
                goto cleanup;
            }
            new_entry = checkout_entry_create(relative_path, new_node->hash);
            free(relative_path);
            if (new_entry == NULL) {
                status = -1;
                goto cleanup;
            }
            (*target_entries)[*entry_count] = new_entry;
            (*entry_count)++;
        }
    }

cleanup:
    fclose(current_tree_object_file);
    return (status);
}
