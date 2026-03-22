#include <ctype.h>
#include <fnmatch.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gitignore.h"
#include "hash.h"
#include "helpers/file_io.h"
#include "services.h"

enum e_gitignore_constants {
    GITIGNORE_LINE_BUFFER_SIZE = 4096,
    GITIGNORE_INDEX_LINE_EXTRA = 8,
};

static void gitignore_reset(gitignore *ignore);
static void gitignore_trim_line(char *line);
static int gitignore_append_rule(gitignore *ignore, const char *pattern,
    int dir_only);
static int gitignore_rule_matches(const gitignore_rule *rule,
    const char *relative_path, const char *entry_name, int is_dir);
static int gitignore_is_tracked_file(const gitignore *ignore,
    const char *relative_path);
static int gitignore_has_tracked_descendants(const gitignore *ignore,
    const char *relative_path);

int gitignore_load(const char *cwd, gitignore *ignore) {
    FILE *file;
    char *ignore_path;
    char line[GITIGNORE_LINE_BUFFER_SIZE];

    if (cwd == NULL || ignore == NULL) {
        return (-1);
    }
    gitignore_reset(ignore);
    ignore->index_path = generate_path(cwd, ".mygit/index");
    if (ignore->index_path == NULL) {
        return (-1);
    }
    ignore_path = generate_path(cwd, ".mygitignore");
    if (ignore_path == NULL) {
        gitignore_destroy(ignore);
        return (-1);
    }
    file = fopen(ignore_path, "r");
    free(ignore_path);
    if (file == NULL) {
        return (0);
    }
    while (fgets(line, sizeof(line), file) != NULL) {
        int dir_only;
        size_t len;

        gitignore_trim_line(line);
        if (line[0] == '\0' || line[0] == '#') {
            continue;
        }
        len = strlen(line);
        dir_only = 0;
        if (len > 0 && line[len - 1] == '/') {
            line[len - 1] = '\0';
            dir_only = 1;
        }
        if (line[0] == '\0') {
            continue;
        }
        if (gitignore_append_rule(ignore, line, dir_only) != 0) {
            fclose(file);
            gitignore_destroy(ignore);
            return (-1);
        }
    }
    fclose(file);
    return (0);
}

void gitignore_destroy(gitignore *ignore) {
    if (ignore == NULL) {
        return;
    }
    for (int i = 0; i < ignore->count; i++) {
        free(ignore->rules[i].pattern);
    }
    free(ignore->rules);
    free(ignore->index_path);
    gitignore_reset(ignore);
}

int gitignore_should_skip(const gitignore *ignore, const char *relative_path,
    const char *entry_name, int is_dir) {
    if (ignore == NULL || relative_path == NULL || entry_name == NULL) {
        return (0);
    }
    for (int i = 0; i < ignore->count; i++) {
        if (gitignore_rule_matches(&ignore->rules[i], relative_path,
                entry_name, is_dir) == 0) {
            continue;
        }
        if (is_dir != 0) {
            if (gitignore_has_tracked_descendants(ignore, relative_path) != 0) {
                return (0);
            }
            return (1);
        }
        if (gitignore_is_tracked_file(ignore, relative_path) != 0) {
            return (0);
        }
        return (1);
    }
    return (0);
}

static void gitignore_reset(gitignore *ignore) {
    ignore->rules = NULL;
    ignore->count = 0;
    ignore->index_path = NULL;
}

static void gitignore_trim_line(char *line) {
    char *start;
    size_t len;

    if (line == NULL) {
        return;
    }
    len = strlen(line);
    while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r'
            || isspace((unsigned char)line[len - 1]) != 0)) {
        line[len - 1] = '\0';
        len--;
    }
    start = line;
    while (*start != '\0' && isspace((unsigned char)*start) != 0) {
        start++;
    }
    if (start != line) {
        memmove(line, start, strlen(start) + 1);
    }
}

static int gitignore_append_rule(gitignore *ignore, const char *pattern,
    int dir_only) {
    gitignore_rule *new_rules;
    char *copy;

    copy = malloc(strlen(pattern) + 1);
    if (copy == NULL) {
        return (-1);
    }
    strcpy(copy, pattern);
    new_rules = realloc(ignore->rules,
        (size_t)(ignore->count + 1) * sizeof(gitignore_rule));
    if (new_rules == NULL) {
        free(copy);
        return (-1);
    }
    ignore->rules = new_rules;
    ignore->rules[ignore->count].pattern = copy;
    ignore->rules[ignore->count].dir_only = dir_only;
    ignore->count++;
    return (0);
}

static int gitignore_rule_matches(const gitignore_rule *rule,
    const char *relative_path, const char *entry_name, int is_dir) {
    const char *pattern;
    size_t pattern_len;

    if (rule == NULL || rule->pattern == NULL || relative_path == NULL
            || entry_name == NULL) {
        return (0);
    }
    pattern = rule->pattern;
    if (pattern[0] == '/') {
        pattern++;
    }
    pattern_len = strlen(pattern);
    if (pattern_len == 0) {
        return (0);
    }
    if (rule->dir_only != 0) {
        if (strncmp(relative_path, pattern, pattern_len) == 0
                && (relative_path[pattern_len] == '\0'
                    || relative_path[pattern_len] == '/')) {
            return (1);
        }
        if (is_dir != 0 && fnmatch(pattern, entry_name, 0) == 0) {
            return (1);
        }
        return (0);
    }
    if (strchr(pattern, '/') != NULL) {
        return (fnmatch(pattern, relative_path, FNM_PATHNAME) == 0);
    }
    return (fnmatch(pattern, entry_name, 0) == 0);
}

static int gitignore_is_tracked_file(const gitignore *ignore,
    const char *relative_path) {
    char tracked_hash[SHA1_HEX_BUFFER_SIZE];

    if (ignore == NULL || ignore->index_path == NULL || relative_path == NULL) {
        return (0);
    }
    return (file_io_read_index_hash(ignore->index_path, relative_path,
            tracked_hash) == 1);
}

static int gitignore_has_tracked_descendants(const gitignore *ignore,
    const char *relative_path) {
    FILE *index_file;
    char line[4096 + SHA1_HEX_BUFFER_SIZE + GITIGNORE_INDEX_LINE_EXTRA];
    size_t prefix_len;

    if (ignore == NULL || ignore->index_path == NULL || relative_path == NULL) {
        return (0);
    }
    index_file = fopen(ignore->index_path, "r");
    if (index_file == NULL) {
        return (0);
    }
    prefix_len = strlen(relative_path);
    while (fgets(line, sizeof(line), index_file) != NULL) {
        char *tab;

        tab = strchr(line, '\t');
        if (tab == NULL) {
            continue;
        }
        if (strncmp(line, relative_path, prefix_len) == 0
                && line[prefix_len] == '/') {
            fclose(index_file);
            return (1);
        }
    }
    fclose(index_file);
    return (0);
}
