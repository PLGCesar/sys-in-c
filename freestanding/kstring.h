#ifndef KSTRING_H
#define KSTRING_H

#include <stddef.h>
#include <stdint.h>

/* String length */
size_t kstrlen(const char *str);

/* String comparison */
int kstrcmp(const char *s1, const char *s2);
int kstrncmp(const char *s1, const char *s2, size_t n);

/* String copy */
char *kstrcpy(char *dest, const char *src);
char *kstrncpy(char *dest, const char *src, size_t n);

/* String concatenation */
char *kstrcat(char *dest, const char *src);
char *kstrncat(char *dest, const char *src, size_t n);

/* Character and substring search */
char *kstrchr(const char *str, int c);
char *kstrstr(const char *haystack, const char *needle);

/* Memory operations */
int kmemcmp(const void *s1, const void *s2, size_t n);
void *kmemmove(void *dest, const void *src, size_t n);

/* Integer to ASCII string conversion */
size_t kitoa(int64_t val, char *buf, int base, int uppercase);

/* ASCII string to integer parsing */
int64_t katoi(const char *str);

/* String to unsigned long with base parsing (hex/octal/dec) */
uint64_t kstrtoul(const char *str, char **endptr, int base);

#endif
