#ifndef MYGIT_GITIGNORE_H
#define MYGIT_GITIGNORE_H

typedef struct s_gitignore_rule {
    char *pattern;
    int dir_only;
} gitignore_rule;

typedef struct s_gitignore {
    gitignore_rule *rules;
    int count;
    char *index_path;
} gitignore;

int gitignore_load(const char *cwd, gitignore *ignore);
void gitignore_destroy(gitignore *ignore);
int gitignore_should_skip(const gitignore *ignore, const char *relative_path,
    const char *entry_name, int is_dir);

#endif
