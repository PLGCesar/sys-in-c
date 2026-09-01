#include "low.h"

#include <stdlib.h>
#include <unistd.h>

static int is_executable(const char *path) {
    return access(path, X_OK) == 0;
}

static void print_help(const char *program) {
    printf("Usage: %s [OPTION]... COMMAND [COMMAND ...]\n\n", program);
    printf("Locate executable commands by searching the directories in PATH.\n");
    printf("For each command, prints the first executable found.\n\n");
    printf("Options:\n");
    printf("  -c, --color  Print command names and results with ANSI colors.\n");
    printf("  --help       Show this help message.\n");
    printf("  -example     Show a usage example.\n\n");
    printf("GNU which-compatible behavior:\n");
    printf("  Multiple commands can be queried in one invocation.\n");
    printf("  A command containing '/' is checked as a path directly.\n");
    printf("  An unset or empty PATH is handled without crashing.\n\n");
    printf("Examples:\n");
    printf("  %s gcc\n", program);
    printf("  %s gcc make\n", program);
    printf("  %s -c gcc\n", program);
    printf("  %s /usr/bin/gcc\n", program);
}

static void print_example(const char *program) {
    printf("Example:\n");
    printf("  $ %s gcc make\n", program);
    printf("  /usr/bin/gcc\n");
    printf("  /usr/bin/make\n\n");
    printf("Use -c or --color to print the results with ANSI colors.\n");
}

static void print_result(const char *path, int color) {
    if (color) {
        printf("\033[32m%s\033[0m\n", path);
    } else {
        puts(path);
    }
}

static void print_not_found(const char *program, const char *command, int color) {
    if (color) {
        fprintf(stderr, "%s: \033[31m%s\033[0m not found\n", program, command);
    } else {
        fprintf(stderr, "%s: %s not found\n", program, command);
    }
}

int main(int argc, char **argv) {
    int color = 0;
    int first_command = 1;

    if (argc < 2) {
        low_print_banner("which");
        print_help(argv[0]);
        return 2;
    }

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--help") == 0) {
            low_print_banner("which");
            print_help(argv[0]);
            return 0;
        }

        if (strcmp(argv[i], "-example") == 0) {
            low_print_banner("which");
            print_example(argv[0]);
            return 0;
        }

        if (strcmp(argv[i], "-c") == 0 || strcmp(argv[i], "--color") == 0) {
            color = 1;
            first_command++;
            continue;
        }

        break;
    }

    if (first_command >= argc) {
        low_print_banner("which");
        print_help(argv[0]);
        return 2;
    }

    const char *path_env = getenv("PATH");
    if (path_env == NULL || *path_env == '\0') {
        for (int arg = first_command; arg < argc; ++arg) {
            const char *command = argv[arg];
            if (strchr(command, '/') != NULL && is_executable(command)) {
                print_result(command, color);
            } else {
                print_not_found(argv[0], command, color);
            }
        }
        return 1;
    }

    int found_any = 0;

    for (int arg = first_command; arg < argc; ++arg) {
        const char *command = argv[arg];

        if (strchr(command, '/') != NULL) {
            if (is_executable(command)) {
                print_result(command, color);
                found_any = 1;
            } else {
                print_not_found(argv[0], command, color);
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
                print_result(candidate, color);
                found = 1;
                found_any = 1;
                free(candidate);
                break;
            }

            free(candidate);
        }

        free(path_copy);

        if (!found) {
            print_not_found(argv[0], command, color);
        }
    }

    return found_any ? 0 : 1;
}
