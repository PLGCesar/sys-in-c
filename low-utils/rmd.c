#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>
#include <time.h>
#include <errno.h>
#include "low.h"

#define CHUNK_SIZE 65536
#define COLOR_RESET "\033[0m"
#define COLOR_OK    "\033[1;32m"
#define COLOR_ERR   "\033[1;31m"
#define COLOR_FILE  "\033[1;36m"

static void print_help(void) {
    low_print_banner("rmd");
    printf("%sUSAGE:%s\n", LOW_COLOR_LABEL, LOW_COLOR_RESET);
    printf("  ./rmd [OPTIONS] <FILE...>\n\n");
    printf("%sDESCRIPTION:%s\n", LOW_COLOR_LABEL, LOW_COLOR_RESET);
    printf("  Anti-forensic file shredder: multi-pass wipe, name obfuscation, and recursion.\n\n");
    printf("%sOPTIONS:%s\n", LOW_COLOR_LABEL, LOW_COLOR_RESET);
    printf("  %s-r, -R, --recursive%s  Remove directories and their contents recursively\n", LOW_COLOR_BIN, LOW_COLOR_RESET);
    printf("  %s-p, --passes <N>%s     Number of wipe passes (1=zeros, 3=DoD zeros/ones/urandom) [Default: 1]\n", LOW_COLOR_BIN, LOW_COLOR_RESET);
    printf("  %s-f, --force%s          Ignore nonexistent files and never prompt\n", LOW_COLOR_BIN, LOW_COLOR_RESET);
    printf("  %s--no-preserve-root%s   Do not treat '/' specially (dangerous)\n", LOW_COLOR_BIN, LOW_COLOR_RESET);
    printf("  %s-h, --help%s           Display this formatted help guide and exit\n", LOW_COLOR_BIN, LOW_COLOR_RESET);
    printf("  %s-v, --version%s        Display version and repository information\n\n", LOW_COLOR_BIN, LOW_COLOR_RESET);
    printf("%sANTI-FORENSIC PIPELINE:%s\n", LOW_COLOR_LABEL, LOW_COLOR_RESET);
    printf("  1. Overwrites file blocks with N patterns (0x00, 0xFF, /dev/urandom)\n");
    printf("  2. Calls fsync() syscall to force disk controller flush from RAM cache\n");
    printf("  3. Obfuscates filename in directory inode table before unlinking\n\n");
    printf("%sEXAMPLES:%s\n", LOW_COLOR_LABEL, LOW_COLOR_RESET);
    printf("  • %s./rmd secret.key%s               (Trituracao rapida 1-pass)\n", LOW_COLOR_TAG, LOW_COLOR_RESET);
    printf("  • %s./rmd -p 3 -r ./pasta_sigilosa%s (Tritura pasta com 3-passes militar)\n\n", LOW_COLOR_TAG, LOW_COLOR_RESET);
}

static void obfuscate_and_unlink(const char *filepath) {
    char dir_part[1024] = ".";
    const char *last_slash = strrchr(filepath, '/');
    if (last_slash) {
        size_t dlen = last_slash - filepath;
        if (dlen < sizeof(dir_part)) {
            strncpy(dir_part, filepath, dlen);
            dir_part[dlen] = '\0';
        }
    }

    char obf_path[2048];
    snprintf(obf_path, sizeof(obf_path), "%s/tmp_shred_%ld_%d", dir_part, time(NULL), rand() % 99999);
    rename(filepath, obf_path);
    unlink(obf_path);
}

static int secure_shred_file(const char *filepath, int passes, int force) {
    struct stat st;
    if (lstat(filepath, &st) < 0) {
        if (!force) fprintf(stderr, "  %s[ERRO]%s %s: %s\n", COLOR_ERR, COLOR_RESET, filepath, strerror(errno));
        return -1;
    }

    if (S_ISLNK(st.st_mode)) {
        if (unlink(filepath) == 0) {
            printf("  %s[OK: LINK REMOVIDO]%s %s%s%s\n", COLOR_OK, COLOR_RESET, COLOR_FILE, filepath, COLOR_RESET);
            return 0;
        }
        return -1;
    }

    if (S_ISREG(st.st_mode) && st.st_size > 0) {
        int fd = open(filepath, O_WRONLY);
        if (fd < 0) {
            if (!force) fprintf(stderr, "  %s[ERRO]%s %s: %s\n", COLOR_ERR, COLOR_RESET, filepath, strerror(errno));
            return -1;
        }

        int rand_fd = open("/dev/urandom", O_RDONLY);
        char buf[CHUNK_SIZE];

        for (int p = 1; p <= passes; p++) {
            lseek(fd, 0, SEEK_SET);
            off_t remaining = st.st_size;

            if (p == 1) memset(buf, 0x00, sizeof(buf));
            else if (p == 2) memset(buf, 0xFF, sizeof(buf));

            while (remaining > 0) {
                size_t to_write = (remaining > CHUNK_SIZE) ? CHUNK_SIZE : remaining;
                if (p >= 3 && rand_fd >= 0) {
                    read(rand_fd, buf, to_write);
                }
                ssize_t w = write(fd, buf, to_write);
                if (w < 0) break;
                remaining -= w;
            }
            fsync(fd);
        }

        if (rand_fd >= 0) close(rand_fd);
        close(fd);
    }

    obfuscate_and_unlink(filepath);
    printf("  %s[OK: SHREDDED]%s      %s%s%s (%d-passes | %lld bytes)\n",
           COLOR_OK, COLOR_RESET, COLOR_FILE, filepath, COLOR_RESET, passes, (long long)st.st_size);

    return 0;
}

static int rmd_recursive(const char *dir_path, int passes, int force) {
    DIR *dir = opendir(dir_path);
    if (!dir) {
        if (!force) fprintf(stderr, "  %s[ERRO]%s Nao foi possivel abrir pasta '%s': %s\n", COLOR_ERR, COLOR_RESET, dir_path, strerror(errno));
        return -1;
    }

    struct dirent *entry;
    char path[2048];

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
        snprintf(path, sizeof(path), "%s/%s", dir_path, entry->d_name);

        struct stat st;
        if (lstat(path, &st) == 0 && S_ISDIR(st.st_mode)) {
            rmd_recursive(path, passes, force);
        } else {
            secure_shred_file(path, passes, force);
        }
    }

    closedir(dir);
    if (rmdir(dir_path) == 0) {
        printf("  %s[OK: DIR REMOVIDO]%s  %s%s%s\n", COLOR_OK, COLOR_RESET, COLOR_FILE, dir_path, COLOR_RESET);
    }
    return 0;
}

int main(int argc, char *argv[]) {
    int recursive = 0, force = 0, passes = 1, preserve_root = 1;
    const char *targets[256];
    int target_count = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0 ||
            strcmp(argv[i], "--version") == 0 || strcmp(argv[i], "-v") == 0) {
            print_help();
            return 0;
        }

        if (strcmp(argv[i], "-r") == 0 || strcmp(argv[i], "-R") == 0 || strcmp(argv[i], "--recursive") == 0) recursive = 1;
        else if (strcmp(argv[i], "-f") == 0 || strcmp(argv[i], "--force") == 0) force = 1;
        else if (strcmp(argv[i], "--no-preserve-root") == 0) preserve_root = 0;
        else if (strcmp(argv[i], "-p") == 0 || strcmp(argv[i], "--passes") == 0) {
            if (i + 1 < argc) {
                passes = atoi(argv[++i]);
                if (passes <= 0) passes = 1;
                if (passes > 10) passes = 10;
            }
        } else {
            if (target_count < 256) targets[target_count++] = argv[i];
        }
    }

    if (target_count == 0) {
        print_help();
        return 1;
    }

    srand(time(NULL));
    int has_errors = 0;

    for (int i = 0; i < target_count; i++) {
        if (preserve_root && (strcmp(targets[i], "/") == 0 || strcmp(targets[i], "/etc") == 0)) {
            fprintf(stderr, "  %s[SEGURANCA]%s Destruicao de '%s' bloqueada! (use --no-preserve-root)\n", COLOR_ERR, COLOR_RESET, targets[i]);
            return 1;
        }

        struct stat st;
        if (lstat(targets[i], &st) == 0 && S_ISDIR(st.st_mode)) {
            if (recursive) {
                if (rmd_recursive(targets[i], passes, force) < 0) has_errors = 1;
            } else {
                fprintf(stderr, "  %s[ERRO]%s '%s' e um diretorio (use -r para recursao)\n", COLOR_ERR, COLOR_RESET, targets[i]);
                has_errors = 1;
            }
        } else {
            if (secure_shred_file(targets[i], passes, force) < 0) has_errors = 1;
        }
    }

    return has_errors ? 1 : 0;
}
