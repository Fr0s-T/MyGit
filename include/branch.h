#ifndef MY_GIT_BRANCH_H
#define MY_GIT_BRANCH_H

int branch(int argc, char **argv);
int branch_load_names(char ***names_out, int *count_out);
void branch_destroy_names(char **names, int count);

#endif
