#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <linux/limits.h>

#include "colors.h"
#include "node.h"

enum e_node_constants {
    NODE_CHILDREN_INITIAL_CAPACITY = 2,
    NODE_CHILDREN_GROWTH_FACTOR = 2,
};

static char *dup_or_null(const char *s) {
    char *copy;

    if (s == NULL)
        return (NULL);
    copy = malloc(strlen(s) + 1);
    if (copy == NULL)
        return (NULL);
    strcpy(copy, s);
    return (copy);
}

static void print_tree_recursive(node *root, int depth, int *branch_state,
    int is_last_child);
static void print_tree_label(node *root);

node *node_create(const char *name, const char *hash,
    node_type type, node *parent) {
    node *new_node;

    new_node = malloc(sizeof(node));
    if (new_node == NULL)
        return (NULL);
    new_node->name = dup_or_null(name);
    if (name != NULL && new_node->name == NULL) {
        free(new_node);
        return (NULL);
    }
    new_node->hash = dup_or_null(hash);
    if (hash != NULL && new_node->hash == NULL) {
        free(new_node->name);
        free(new_node);
        return (NULL);
    }
    new_node->type = type;
    new_node->parent = parent;
    new_node->children_count = 0;
    new_node->children_capacity = 0;
    new_node->children = NULL;
    return (new_node);
}

void node_destroy(node *root) {
    int i;

    if (root == NULL)
        return ;
    i = 0;
    while (i < root->children_count) {
        node_destroy(root->children[i]);
        i++;
    }
    free(root->children);
    free(root->name);
    free(root->hash);
    free(root);
}

node *find_child(node *current_node, char *child_name) {
    if (current_node == NULL || child_name == NULL) {
        return (NULL);
    }
    if (current_node->children_count < 0) {
        return (NULL);
    }
    if (current_node->children_count > 0 && current_node->children == NULL) {
        return (NULL);
    }
    for (int i = 0; i < current_node->children_count; i++) {
        if (current_node->children[i] == NULL) {
            continue;
        }
        if (current_node->children[i]->name == NULL) {
            continue;
        }
        if (strcmp(current_node->children[i]->name, child_name) == 0) {
            return (current_node->children[i]);
        }
    }
    return (NULL);
}

/**
 *0 for full tree
 *
*/
void print_tree(node *root, int depth) {
    int branch_state[PATH_MAX];

    if (root == NULL) {
        return ;
    }
    for (int i = 0; i < PATH_MAX; i++) {
        branch_state[i] = 0;
    }
    print_tree_recursive(root, depth, branch_state, 1);
}

static void print_tree_recursive(node *root, int depth, int *branch_state,
    int is_last_child) {
    if (root == NULL) {
        return ;
    }
    if (depth > 0) {
        for (int i = 0; i < depth - 1; i++) {
            if (branch_state[i] != 0) {
                printf(C_CYAN "|   " C_RESET);
            }
            else {
                printf("    ");
            }
        }
        if (is_last_child != 0) {
            printf(C_CYAN "`-- " C_RESET);
        }
        else {
            printf(C_CYAN "|-- " C_RESET);
        }
    }
    print_tree_label(root);
    printf("\n\n");
    branch_state[depth] = (is_last_child == 0);
    for (int i = 0; i < root->children_count; i++) {
        print_tree_recursive(root->children[i], depth + 1, branch_state,
            i == root->children_count - 1);
    }
}

static void print_tree_label(node *root) {
    if (root->name == NULL) {
        printf(C_YELLOW "(null)" C_RESET);
    }
    else if (root->name[0] == '\0') {
        printf(C_BOLD C_BLUE "(root)" C_RESET);
    }
    else {
        if (root->type == NODE_DIR || root->type == NODE_ROOT) {
            printf(C_BLUE "%s" C_RESET, root->name);
        }
        else {
            printf(C_GREEN "%s" C_RESET, root->name);
        }
    }
    if (root->hash != NULL) {
        printf(C_YELLOW " [%s]" C_RESET, root->hash);
    }
}

int node_add_child(node *parent, node *child) {
    node **new_children;
    int new_capacity;
    int i;

    if (parent == NULL || child == NULL) {
        return (-1);
    }
    if (parent->children_count < 0 || parent->children_capacity < 0) {
        return (-1);
    }
    if (parent->children_count > parent->children_capacity) {
        return (-1);
    }
    if (parent->children_count == parent->children_capacity) {
        /* Geometric growth keeps inserts cheap without reallocating on every child. */
        if (parent->children_capacity == 0) {
            new_capacity = NODE_CHILDREN_INITIAL_CAPACITY;
        }
        else {
            new_capacity = parent->children_capacity * NODE_CHILDREN_GROWTH_FACTOR;
        }
        new_children = malloc(sizeof(node *) * new_capacity);
        if (new_children == NULL) {
            return (-1);
        }
        i = 0;
        while (i < parent->children_count) {
            new_children[i] = parent->children[i];
            i++;
        }
        free(parent->children);
        parent->children = new_children;
        parent->children_capacity = new_capacity;
    }
    parent->children[parent->children_count] = child;
    parent->children_count++;
    child->parent = parent;
    return (0);
}

int get_nodes_path(node *child, char **node_path) {
    char *new_path;
    size_t current_len;
    size_t child_len;
    size_t needed;

    if (child == NULL || node_path == NULL) {
        return (-1);
    }
    if (child->type == NODE_ROOT) {
        *node_path = malloc(1);
        if (*node_path == NULL) {
            return (-1);
        }
        (*node_path)[0] = '\0';
        return (0);
    }
    if (get_nodes_path(child->parent, node_path) != 0) {
        return (-1);
    }
    if (*node_path == NULL || child->name == NULL) {
        free(*node_path);
        *node_path = NULL;
        return (-1);
    }
    current_len = strlen(*node_path);
    child_len = strlen(child->name);
    needed = current_len + child_len + (current_len > 0 ? 1 : 0) + 1;
    new_path = realloc(*node_path, needed);
    if (new_path == NULL) {
        free(*node_path);
        *node_path = NULL;
        return (-1);
    }
    *node_path = new_path;
    if (current_len > 0) {
        snprintf(*node_path + current_len, needed - current_len, "/%s", child->name);
    }
    else {
        snprintf(*node_path, needed, "%s", child->name);
    }
    return (0);
}
