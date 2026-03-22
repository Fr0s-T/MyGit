#ifndef MYGIT_GITIGNORE_H
#define MYGIT_GITIGNORE_H

typedef struct s_gitignore_rule {
    /* Rule pattern string, owned by the containing gitignore instance. */
    char *pattern;
    /* Non-zero when the rule only applies to directories. */
    int dir_only;
} gitignore_rule;

typedef struct s_gitignore {
    /* Heap array of loaded rules, owned by this struct. */
    gitignore_rule *rules;
    /* Number of valid entries in rules. */
    int count;
    /* Heap string to `.mygit/index`, owned by this struct. */
    char *index_path;
} gitignore;

/*
** Loads `.mygitignore` rules for a repository root.
**
** Returns:
** - 0 on success, including when no `.mygitignore` file exists
** - -1 on allocation or read failure
**
** Ownership:
** - fills ignore with heap-owned internal state
** - caller must later call gitignore_destroy()
*/
int gitignore_load(const char *cwd, gitignore *ignore);

/*
** Frees all heap-owned state inside a loaded gitignore struct.
*/
void gitignore_destroy(gitignore *ignore);

/*
** Returns non-zero when an entry should be skipped during traversal.
**
** Parameters:
** - relative_path: repo-relative path for the candidate entry
** - entry_name: basename of the candidate entry
** - is_dir: non-zero for directories, zero for regular files
**
** Ownership:
** - borrows all pointers
*/
int gitignore_should_skip(const gitignore *ignore, const char *relative_path,
    const char *entry_name, int is_dir);

#endif
