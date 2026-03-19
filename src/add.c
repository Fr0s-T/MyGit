#include "../include/my_includes.h"

static int input_check(int argc, char **argv);

int add(int argc, char **argv) {
	char cwd[PATH_MAX];
	file_data **files = NULL;
	int len_files = 0;

	if (input_check(argc, argv) == -1) {
		return -1;
	}

	if (getcwd(cwd, sizeof(cwd)) == NULL) {
		printf("\nFailed to get working dir\n");
		return -1;
	}

	printf("cwd: %s\n", cwd);

	if (traverse_directory(cwd, &files, &len_files,cwd) == -1) {
		return -1;
	}

	for (int i = 0; i < len_files; i++) {
		printf("\n[From add] the file is :%s and its hash:%s",
			files[i]->path,
			files[i]->hash);
	}

	printf("\nTotal files found: %d\n", len_files);
	return 0;
}

static int input_check(int argc, char **argv) {
	if (argc > 3) {
		printf("\nToo many args\n");
		return -1;
	}

	if (argc < 3) {
		printf("\nMissing add target\n");
		return -1;
	}

	if (strcmp(argv[2], ".") == 0) {
		return 0;
	}

	printf("\nNot supported or wrong arg\n");
	return -1;
}