#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "colors.h"
#include "helpers/commit_object.h"
#include "helpers/file_io.h"
#include "log.h"

#define LOG_TAG_COLOR C_BLUE
#define LOG_LABEL_COLOR C_GREEN
#define LOG_VALUE_COLOR C_CYAN
#define LOG_ERROR_COLOR C_RED

static char *duplicate_string(const char *value);
static void print_commit_log(const char *commit_hash,
    const commit_object_info *info);

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
    while (current_commit_hash[0] != '\0') {
        commit_object_info info;
        char *next_commit_hash;

        next_commit_hash = NULL;
        if (commit_object_read_info(current_commit_hash, &info) != 0) {
            goto cleanup;
        }
        print_commit_log(current_commit_hash, &info);
        if (info.parent_count > 0) {
            next_commit_hash = duplicate_string(info.parent_hashes[0]);
            if (next_commit_hash == NULL) {
                commit_object_destroy_info(&info);
                goto cleanup;
            }
        }
        commit_object_destroy_info(&info);
        free(current_commit_hash);
        if (next_commit_hash == NULL) {
            current_commit_hash = duplicate_string("");
        }
        else {
            current_commit_hash = next_commit_hash;
        }
        if (current_commit_hash == NULL) {
            goto cleanup;
        }
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

static void print_commit_log(const char *commit_hash,
    const commit_object_info *info) {
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
