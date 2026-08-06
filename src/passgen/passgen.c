#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

// Character set containing uppercase, lowercase, numbers, and symbols
const char CHARSET[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789!@#$%^&*()-_=+[]{};:,.<>/?";

int main(int argc, char *argv[]) {
    int length = 16; // Default password length

    // Check if the user provided a custom length
    if (argc > 1) {
        length = atoi(argv[1]);
        if (length <= 0 || length > 1024) {
            printf("Error: Invalid password length. Please use a number between 1 and 1024.\n");
            return 1;
        }
    }

    // Open the system's cryptographically secure random number generator
    int fd = open("/dev/urandom", O_RDONLY);
    if (fd < 0) {
        printf("Error: Failed to open /dev/urandom.\n");
        return 1;
    }

    // Array to store the true random bytes
    unsigned char *random_bytes = malloc(length);
    if (random_bytes == NULL) {
        printf("Error: Memory allocation failed.\n");
        close(fd);
        return 1;
    }

    // Read the exact number of bytes requested
    if (read(fd, random_bytes, length) != length) {
        printf("Error: Failed to read secure random bytes.\n");
        free(random_bytes);
        close(fd);
        return 1;
    }
    
    close(fd); // Close the file descriptor

    int charset_size = sizeof(CHARSET) - 1; // -1 to ignore the null terminator

    // Print the generated password
    printf("======================\n");
    printf("[Password]: ");
    
    for (int i = 0; i < length; i++) {
        // Map each random byte to a character in our CHARSET
        printf("%c", CHARSET[random_bytes[i] % charset_size]);
    }
    
    printf("\n======================\n");

    free(random_bytes);
    return 0;
}
