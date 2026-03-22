#include <stdio.h>
#include <string.h>

#include "add.h"
#include "branch.h"
#include "checkout.h"
#include "commit.h"
#include "init.h"
#include "log.h"
#include "merge.h"
#include "reset.h"

int router(int argc, char **argv);
static void print_help(void);

int main(int argc, char **argv) {
    if (router(argc, argv) != 0) {
        printf("\nfatal error\n");
        return -1;
    }

    return 0;
}

int router(int argc, char **argv) {
    if (argc == 1) {
        printf("\nNo command has been enterd\n");
        printf("Use `mygit -help` for a quick guide.\n");
        return -1;
    }
    else if (strcmp(argv[1], "-help") == 0 || strcmp(argv[1], "--help") == 0) {
        print_help();
        return 0;
    }
    else if (strcmp(argv[1], "init") == 0) {
        printf("\nInit has been called. Initilizing local git repo\n");
        return init(argc);
    }
    else if (strcmp(argv[1], "add") == 0) {
        printf("\nAdd has been called. Tracking changes...\n");
        return add(argc, argv);
    }
    else if (strcmp(argv[1], "commit") == 0) {
        printf("commit has been called\n");
        return commit(argc, argv);
    }
    else if (strcmp(argv[1], "log") == 0) {
        return log_cmd(argc, argv);
    }
    else if (strcmp(argv[1], "merge") == 0) {
        return merge_cmd(argc, argv);
    }
    else if (strcmp(argv[1], "branch") == 0) {
        return branch(argc, argv);
    }
    else if (strcmp(argv[1], "checkout") == 0) {
        return checkout(argc, argv);
    }
    else if (strcmp(argv[1], "reset") == 0) {
        return reset_cmd(argc, argv);
    }
    printf("\nUnknown command: %s\n", argv[1]);
    printf("Use `mygit -help` for a quick guide.\n");
    return -1;
}

static void print_help(void) {
    printf("\nMyGit quick guide\n\n");
    printf("Commands:\n");
    printf("  mygit init\n");
    printf("      create a new .mygit repository\n\n");
    printf("  mygit add .\n");
    printf("      stage the current working tree snapshot\n\n");
    printf("  mygit commit -m \"message\"\n");
    printf("      create a commit from the index\n\n");
    printf("  mygit log\n");
    printf("      print commit history from the current branch\n\n");
    printf("  mygit branch\n");
    printf("      list branches\n\n");
    printf("  mygit branch <name>\n");
    printf("      create a branch at the current ref\n\n");
    printf("  mygit branch -d <name>\n");
    printf("      delete a branch if it is not current\n\n");
    printf("  mygit checkout <branch>\n");
    printf("      switch working tree and HEAD to a branch\n\n");
    printf("  mygit reset -r <commit-hash>\n");
    printf("      move current branch and tracked state to a commit\n\n");
    printf("  mygit merge <branch>\n");
    printf("      merge a branch into the current branch\n\n");
    printf("  mygit merge -i <branch>\n");
    printf("      resolve merge conflicts by preferring incoming files\n\n");
    printf("  mygit merge -c <branch>\n");
    printf("      resolve merge conflicts by keeping current files\n");
}
