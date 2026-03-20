#ifndef MYGIT_HELPERS_COMMIT_TREE_H
#define MYGIT_HELPERS_COMMIT_TREE_H

#include "../node.h"

int build_tree_pass_one(char *index_path, node *root);
int build_tree_second_pass_recursive_ascent(node *root);

#endif
