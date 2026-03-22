#ifndef MY_GIT_NODE
#define MY_GIT_NODE

#include <limits.h>

typedef enum e_node_type {
    NODE_ROOT,
    NODE_UNKNOWN,
    NODE_FILE,
    NODE_DIR
} node_type;

typedef struct s_node {
    char *name;
    char *hash;
    node_type type;
    struct s_node *parent;
    int children_count;
    int children_capacity;
    struct s_node **children;
} node;

node *node_create(const char *name, const char *hash,
    node_type type, node *parent);
void node_destroy(node *root);
node *find_child(node *current_node, char *child_name);
void print_tree(node *root, int depth);
int node_add_child(node *parent, node *child);

/*
** Builds the repo-relative path for a node by walking up through its parents.
**
** Examples:
** - root child "src" -> "src"
** - nested file under src -> "src/main.c"
**
** Returns:
**  0  on success
** -1  on allocation or invalid-input failure
**
** Ownership:
** - allocates *node_path on success
** - caller must free(*node_path)
*/
int get_nodes_path(node *child, char **node_path);

#endif
