#include "low.h"

#include <errno.h>
#include <stdlib.h>
#include <unistd.h>

static int is_executable(const char *path) {
    return access(path, X_OK) == 0;
}

int main(int argc, char **argv) {
    low_print_banner("which");

    if (argc < 2) {
        fprintf(stderr, "Usage: %s COMMAND [COMMAND ...]\n", argv[0]);
        return 2;
    }

    const char *path_env = getenv("PATH");
    if (path_env == NULL) {
        fprintf(stderr, "%s: PATH is not set\n", argv[0]);
        return 1;
    }

    int found_any = 0;

    for (int arg = 1; arg < argc; ++arg) {
        const char *command = argv[arg];

        /* If the command already contains '/', check it directly. */
        if (strchr(command, '/') != NULL) {
            if (is_executable(command)) {
                puts(command);
                found_any = 1;
            }
            continue;
        }

        char *path_copy = strdup(path_env);
        if (path_copy == NULL) {
            perror("strdup");
            return 1;
        }

        int found = 0;
        char *saveptr = NULL;
        for (char *dir = strtok_r(path_copy, ":", &saveptr);
             dir != NULL;
             dir = strtok_r(NULL, ":", &saveptr)) {
            const char *base = (*dir == '\0') ? "." : dir;
            size_t needed = strlen(base) + 1 + strlen(command) + 1;
            char *candidate = malloc(needed);

            if (candidate == NULL) {
                free(path_copy);
                perror("malloc");
                return 1;
            }

            snprintf(candidate, needed, "%s/%s", base, command);

            if (is_executable(candidate)) {
                puts(candidate);
                found = 1;
                found_any = 1;
                free(candidate);
                break;
            }

            free(candidate);
        }

        free(path_copy);

        if (!found) {
            fprintf(stderr, "%s: %s not found\n", argv[0], command);
        }
    }

    return found_any ? 0 : 1;
}
