#ifndef MYGIT_CHECKOUT_PREPARE_H
#define MYGIT_CHECKOUT_PREPARE_H

int checkout_input_check(int argc, char **argv, char **branch_name);
int checkout_branch_exists(const char *branch_name);
int checkout_repo_is_up_to_date_with_branch(void);
int checkout_read_target_commit(const char *branch_name,
    char **target_ref_path, char **target_commit_hash);
int checkout_read_target_root(const char *target_commit_hash,
    char **root_hash, char **root_path);
int checkout_update_head_ref(const char *target_ref_path);

#endif
