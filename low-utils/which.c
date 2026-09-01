#include "low.h"

#include <stdlib.h>
#include <unistd.h>

static int is_executable(const char *path) {
    return access(path, X_OK) == 0;
}

static void print_help(const char *program) {
    printf("Usage: %s COMMAND [COMMAND ...]\n\n", program);
    printf("Find an executable command by searching the directories in PATH.\n");
    printf("For each command, prints the first executable found.\n\n");
    printf("Options:\n");
    printf("  --help       Show this help message.\n");
    printf("  -example     Show a usage example.\n\n");
    printf("Examples:\n");
    printf("  %s gcc\n", program);
    printf("  %s gcc make\n", program);
    printf("  %s /usr/bin/gcc\n", program);
}

static void print_example(const char *program) {
    printf("Example:\n");
    printf("  $ %s gcc\n", program);
    printf("  /usr/bin/gcc\n\n");
    printf("The command is searched in each directory listed in PATH.\n");
}

int main(int argc, char **argv) {
    if (argc < 2) {
        low_print_banner("which");
        print_help(argv[0]);
        return 2;
    }

    if (strcmp(argv[1], "--help") == 0) {
        low_print_banner("which");
        print_help(argv[0]);
        return 0;
    }

    if (strcmp(argv[1], "-example") == 0) {
        low_print_banner("which");
        print_example(argv[0]);
        return 0;
    }

    const char *path_env = getenv("PATH");
    if (path_env == NULL) {
        fprintf(stderr, "%s: PATH is not set\n", argv[0]);
        return 1;
    }

    int found_any = 0;

    for (int arg = 1; arg < argc; ++arg) {
        const char *command = argv[arg];

        if (strchr(command, '/') != NULL) {
            if (is_executable(command)) {
                puts(command);
                found_any = 1;
            } else {
                fprintf(stderr, "%s: %s not found\n", argv[0], command);
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
