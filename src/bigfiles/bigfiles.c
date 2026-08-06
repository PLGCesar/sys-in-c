#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

#define MAX_DIRS 25

int scanned_dir_count = 0;
double min_size_mb = 10.0;

void scan_directory(const char *dir_path) {
    if (scanned_dir_count >= MAX_DIRS) return;
    scanned_dir_count++;

    DIR *dir = opendir(dir_path);
    if (!dir) return;

    struct dirent *entry;
    char path[1024];

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;

        snprintf(path, sizeof(path), "%s/%s", dir_path, entry->d_name);

        struct stat st;
        if (stat(path, &st) == 0) {
            if (S_ISDIR(st.st_mode)) {
                if (scanned_dir_count < MAX_DIRS) {
                    scan_directory(path);
                }
            } else if (S_ISREG(st.st_mode)) {
                double size_mb = (double)st.st_size / (1024.0 * 1024.0);
                if (size_mb >= min_size_mb) {
                    if (size_mb >= 1024.0) {
                        printf("  [%.2f GB] %s\n", size_mb / 1024.0, path);
                    } else {
                        printf("  [%.2f MB] %s\n", size_mb, path);
                    }
                }
            }
        }
    }
    closedir(dir);
}

int main(int argc, char *argv[]) {
    if (argc > 1) {
        min_size_mb = atof(argv[1]);
        if (min_size_mb <= 0) min_size_mb = 10.0;
    }

    printf("======================\n");
    printf("[Search Min Size: %.2f MB | Dir Limit: %d]\n", min_size_mb, MAX_DIRS);
    printf("======================\n");

    scan_directory(".");

    printf("======================\n");
    printf("[Scan finished. Subdirectories scanned: %d]\n", scanned_dir_count);
    printf("======================\n");

    return 0;
}
