#include "../include/my_includes.h"

int		create_blob(file_data *file, char *git_obj_path);
char	*create_git_dir(char *cwd);
int		blob_exists(char *file_hash, char *git_obj_path);
int		copy_file_content(char *src_path, char *dst_path);
int		update_index(file_data *file, char *cwd);

int create_blob_and_indexing(file_data **files, int len_file, char *cwd) {
	char	*git_obj_path;

	printf(C_YELLOW "[create_blob_and_indexing] Creating blobs...\n" C_RESET);
	git_obj_path = create_git_dir(cwd);
	if (git_obj_path == NULL) {
		printf("[create_blob_and_indexing]" C_RED " Failed to create git objects path\n" C_RESET);
		return (-1);
	}
	printf("[create_blob_and_indexing] Objects path: " C_BLUE "%s\n" C_RESET, git_obj_path);
	for (int i = 0; i < len_file; i++) {
		file_data *file = files[i];

		printf("[create_blob_and_indexing] Processing: " C_RED "%s" C_RESET " -> " C_BLUE "%s\n" C_RESET, file->path, file->hash);
		if (create_blob(file, git_obj_path) != 0) {
			printf("[Blob creation]" C_RED " Failed to create blob for: %s\n" C_RESET, file->path);
			continue;
		}
		if (update_index(file, cwd) != 0) {
			printf("[Indexing]" C_RED " Failed to update index for: %s\n" C_RESET, file->path);
		}
	}
	free(git_obj_path);
	printf(C_GREEN "[create_blob_and_indexing] Blob creation phase done.\n" C_RESET);
	return (0);
}

char *create_git_dir(char *cwd) {
	return (generate_path(cwd, ".mygit/objects"));
}

int create_blob(file_data *file, char *git_obj_path) {
	char	blob_path[PATH_MAX];
	int		exists_status;

	printf("[create_blob] Checking blob for: " C_RED "%s\n" C_RESET, file->path);
	exists_status = blob_exists(file->hash, git_obj_path);
	if (exists_status == 0) {
		printf("[create_blob] Blob already exists: " C_BLUE "%s\n" C_RESET, file->hash);
		return (0);
	}
	if (exists_status == -1) {
		printf("[create_blob]" C_RED " Could not check if blob exists for: %s\n" C_RESET, file->path);
		return (-1);
	}
	if (snprintf(blob_path, sizeof(blob_path), "%s/%s", git_obj_path, file->hash) >= (int)sizeof(blob_path)) {
		printf("[create_blob]" C_RED " Blob path too long for: %s\n" C_RESET, file->path);
		return (-1);
	}
	printf("[create_blob] Creating blob: " C_BLUE "%s\n" C_RESET, blob_path);
	if (copy_file_content(file->path, blob_path) != 0) {
		printf("[create_blob]" C_RED " Failed copying data for: %s\n" C_RESET, file->path);
		return (-1);
	}
	printf("[create_blob]" C_GREEN " Blob created successfully for: %s\n" C_RESET, file->path);
	return (0);
}

int copy_file_content(char *src_path, char *dst_path) {
	FILE	*src_file;
	FILE	*dst_file;
	char	buffer[4096];
	size_t	bytes_read;
	size_t	bytes_written;
	size_t	total_written;

	total_written = 0;
	printf("[copy_file_content] Copying from " C_RED "%s" C_RESET " to " C_BLUE "%s\n" C_RESET, src_path, dst_path);
	src_file = fopen(src_path, "rb");
	if (src_file == NULL) {
		printf("[copy_file_content]" C_RED " Failed to open source file: %s\n" C_RESET, src_path);
		return (-1);
	}
	dst_file = fopen(dst_path, "wb");
	if (dst_file == NULL) {
		printf("[copy_file_content]" C_RED " Failed to open destination file: %s\n" C_RESET, dst_path);
		fclose(src_file);
		return (-1);
	}
	bytes_read = fread(buffer, 1, sizeof(buffer), src_file);
	while (bytes_read > 0) {
		bytes_written = fwrite(buffer, 1, bytes_read, dst_file);
		if (bytes_written != bytes_read) {
			printf("[copy_file_content]" C_RED " Failed writing to blob: %s\n" C_RESET, dst_path);
			fclose(src_file);
			fclose(dst_file);
			remove(dst_path);
			return (-1);
		}
		total_written += bytes_written;
		bytes_read = fread(buffer, 1, sizeof(buffer), src_file);
	}
	if (ferror(src_file)) {
		printf("[copy_file_content]" C_RED " Failed reading source file: %s\n" C_RESET, src_path);
		fclose(src_file);
		fclose(dst_file);
		remove(dst_path);
		return (-1);
	}
	fclose(src_file);
	fclose(dst_file);
	printf("[copy_file_content]" C_GREEN " Copy complete. Total bytes written: %zu\n" C_RESET, total_written);
	return (0);
}

int blob_exists(char *file_hash, char *git_obj_path) {
	DIR				*current_dir;
	struct dirent	*entry;

	printf("[blob_exists] Looking for blob: " C_BLUE "%s\n" C_RESET, file_hash);
	current_dir = opendir(git_obj_path);
	if (current_dir == NULL) {
		printf("\n[blob_exists]" C_RED " Failed to open dir: %s\n" C_RESET, git_obj_path);
		return (-1);
	}
	while ((entry = readdir(current_dir)) != NULL) {
		if (strcmp(file_hash, entry->d_name) == 0) {
			closedir(current_dir);
			printf("[blob_exists]" C_GREEN " Blob found: %s\n" C_RESET, file_hash);
			return (0);
		}
	}
	closedir(current_dir);
	printf("[blob_exists] Blob not found: " C_YELLOW "%s\n" C_RESET, file_hash);
	return (1);
}

int update_index(file_data *file, char *cwd) {
	char	index_path[PATH_MAX];
	char	temp_path[PATH_MAX];
	FILE	*index_file;
	FILE	*temp_file;
	char	line[PATH_MAX + 128];
	char	existing_path[PATH_MAX];
	char	existing_hash[41];
	int		found;

	found = 0;
	if (snprintf(index_path, sizeof(index_path), "%s/.mygit/index", cwd) >= (int)sizeof(index_path)) {
		printf("[update_index]" C_RED " Index path too long\n" C_RESET);
		return (-1);
	}
	if (snprintf(temp_path, sizeof(temp_path), "%s/.mygit/index.tmp", cwd) >= (int)sizeof(temp_path)) {
		printf("[update_index]" C_RED " Temp index path too long\n" C_RESET);
		return (-1);
	}
	printf("[update_index] Updating index for: " C_RED "%s\n" C_RESET, file->path);
	index_file = fopen(index_path, "r");
	temp_file = fopen(temp_path, "w");
	if (temp_file == NULL) {
		printf("[update_index]" C_RED " Failed to open temp index: %s\n" C_RESET, temp_path);
		if (index_file != NULL) {
			fclose(index_file);
		}
		return (-1);
	}
	if (index_file != NULL) {
		while (fgets(line, sizeof(line), index_file) != NULL) {
			if (sscanf(line, "%s %40s", existing_path, existing_hash) == 2) {
				if (strcmp(existing_path, file->path) == 0) {
					fprintf(temp_file, "%s %s\n", file->path, file->hash);
					found = 1;
					printf("[update_index] Replaced entry: " C_RED "%s" C_RESET " -> " C_BLUE "%s\n" C_RESET, file->path, file->hash);
				} else {
					fprintf(temp_file, "%s %s\n", existing_path, existing_hash);
				}
			}
		}
		fclose(index_file);
	}
	if (found == 0) {
		fprintf(temp_file, "%s %s\n", file->path, file->hash);
		printf("[update_index] Added entry: " C_RED "%s" C_RESET " -> " C_BLUE "%s\n" C_RESET, file->path, file->hash);
	}
	fclose(temp_file);
	if (remove(index_path) != 0) {
		printf("[update_index] Old index missing or could not be removed, continuing...\n");
	}
	if (rename(temp_path, index_path) != 0) {
		printf("[update_index]" C_RED " Failed to replace index file\n" C_RESET);
		remove(temp_path);
		return (-1);
	}
	printf("[update_index]" C_GREEN " Index updated successfully for: %s\n" C_RESET, file->path);
	return (0);
}