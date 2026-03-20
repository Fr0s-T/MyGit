#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <linux/limits.h>

#include "../include/colors.h"
#include "../include/helpers/file_io.h"
#include "../include/log.h"

#define LOG_TAG_COLOR C_BLUE
#define LOG_LABEL_COLOR C_GREEN
#define LOG_VALUE_COLOR C_CYAN
#define LOG_ERROR_COLOR C_RED

typedef struct s_commit_info {
    char *tree_hash;
    char *branch_name;
    char *time_value;
    char *parent_hash;
} commit_info;

static char *duplicate_string(const char *value);
static void trim_line_endings(char *value);
static int load_commit_info(const char *commit_hash, commit_info *info);
static void destroy_commit_info(commit_info *info);
static void print_commit_log(const char *commit_hash, const commit_info *info);
static int parse_prefixed_line(const char *line, const char *prefix, char **out);

int log_cmd(int argc, char **argv) {
    char *head_ref_path;
    char *current_commit_hash;
    int status;

    head_ref_path = NULL;
    current_commit_hash = NULL;
    status = -1;
    (void)argv;
    if (argc != 2) {
        printf(LOG_TAG_COLOR "[log]" C_RESET " "
            LOG_ERROR_COLOR "usage: mygit log\n" C_RESET);
        return (-1);
    }
    if (file_io_read_first_line(".mygit/HEAD", &head_ref_path) == -1) {
        printf(LOG_TAG_COLOR "[log]" C_RESET " "
            LOG_ERROR_COLOR "failed to read HEAD\n" C_RESET);
        goto cleanup;
    }
    if (file_io_read_first_line(head_ref_path, &current_commit_hash) == -1) {
        printf(LOG_TAG_COLOR "[log]" C_RESET " "
            LOG_ERROR_COLOR "failed to read current branch ref\n" C_RESET);
        goto cleanup;
    }
    if (current_commit_hash[0] == '\0') {
        printf(LOG_TAG_COLOR "[log]" C_RESET " "
            LOG_ERROR_COLOR "no commits yet\n" C_RESET);
        status = 0;
        goto cleanup;
    }
    while (current_commit_hash[0] != '\0' && strcmp(current_commit_hash, "NULL") != 0) {
        commit_info info;
        char *next_commit_hash;

        info.tree_hash = NULL;
        info.branch_name = NULL;
        info.time_value = NULL;
        info.parent_hash = NULL;
        next_commit_hash = NULL;
        if (load_commit_info(current_commit_hash, &info) == -1) {
            destroy_commit_info(&info);
            goto cleanup;
        }
        print_commit_log(current_commit_hash, &info);
        next_commit_hash = duplicate_string(info.parent_hash);
        destroy_commit_info(&info);
        if (next_commit_hash == NULL) {
            goto cleanup;
        }
        free(current_commit_hash);
        current_commit_hash = next_commit_hash;
    }
    status = 0;

cleanup:
    free(current_commit_hash);
    free(head_ref_path);
    return (status);
}

static char *duplicate_string(const char *value) {
    char *copy;

    if (value == NULL) {
        return (NULL);
    }
    copy = malloc(strlen(value) + 1);
    if (copy == NULL) {
        return (NULL);
    }
    strcpy(copy, value);
    return (copy);
}

static void trim_line_endings(char *value) {
    size_t len;

    if (value == NULL) {
        return ;
    }
    len = strlen(value);
    while (len > 0 && (value[len - 1] == '\n' || value[len - 1] == '\r')) {
        value[len - 1] = '\0';
        len--;
    }
}

static int load_commit_info(const char *commit_hash, commit_info *info) {
    char object_path[PATH_MAX];
    FILE *file;
    char line[PATH_MAX];

    if (commit_hash == NULL || info == NULL) {
        return (-1);
    }
    if (snprintf(object_path, sizeof(object_path), ".mygit/objects/%s", commit_hash) >= (int)sizeof(object_path)) {
        return (-1);
    }
    file = fopen(object_path, "r");
    if (file == NULL) {
        return (-1);
    }
    if (fgets(line, sizeof(line), file) == NULL || parse_prefixed_line(line, "tree ", &info->tree_hash) == -1) {
        fclose(file);
        destroy_commit_info(info);
        return (-1);
    }
    if (fgets(line, sizeof(line), file) == NULL || parse_prefixed_line(line, "branch ", &info->branch_name) == -1) {
        fclose(file);
        destroy_commit_info(info);
        return (-1);
    }
    if (fgets(line, sizeof(line), file) == NULL || parse_prefixed_line(line, "time ", &info->time_value) == -1) {
        fclose(file);
        destroy_commit_info(info);
        return (-1);
    }
    if (fgets(line, sizeof(line), file) == NULL || parse_prefixed_line(line, "parent ", &info->parent_hash) == -1) {
        fclose(file);
        destroy_commit_info(info);
        return (-1);
    }
    fclose(file);
    return (0);
}

static void destroy_commit_info(commit_info *info) {
    if (info == NULL) {
        return ;
    }
    free(info->tree_hash);
    free(info->branch_name);
    free(info->time_value);
    free(info->parent_hash);
}

static void print_commit_log(const char *commit_hash, const commit_info *info) {
    time_t raw_time;
    struct tm *time_info;
    char formatted_time[64];
    long parsed_time;

    parsed_time = strtol(info->time_value, NULL, 10);
    raw_time = (time_t)parsed_time;
    time_info = localtime(&raw_time);
    if (time_info == NULL || strftime(formatted_time, sizeof(formatted_time),
            "%Y-%m-%d %H:%M:%S", time_info) == 0) {
        snprintf(formatted_time, sizeof(formatted_time), "%s", info->time_value);
    }
    printf(LOG_TAG_COLOR "[log]" C_RESET " "
        LOG_LABEL_COLOR "commit: " LOG_VALUE_COLOR "%s\n" C_RESET,
        commit_hash);
    printf(LOG_TAG_COLOR "[log]" C_RESET " "
        LOG_LABEL_COLOR "time:   " LOG_VALUE_COLOR "%s\n" C_RESET,
        formatted_time);
    printf(LOG_TAG_COLOR "[log]" C_RESET " "
        LOG_LABEL_COLOR "branch: " LOG_VALUE_COLOR "%s\n\n" C_RESET,
        info->branch_name);
}

static int parse_prefixed_line(const char *line, const char *prefix, char **out) {
    const char *value;

    if (line == NULL || prefix == NULL || out == NULL) {
        return (-1);
    }
    if (strncmp(line, prefix, strlen(prefix)) != 0) {
        return (-1);
    }
    value = line + strlen(prefix);
    *out = duplicate_string(value);
    if (*out == NULL) {
        return (-1);
    }
    trim_line_endings(*out);
    return (0);
}
