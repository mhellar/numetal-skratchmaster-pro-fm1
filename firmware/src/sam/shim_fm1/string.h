#ifndef STRING_SHIM_H
#define STRING_SHIM_H
void *memcpy(void *d, const void *s, unsigned n);
void *memset(void *d, int c, unsigned n);
unsigned strlen(const char *s);
#endif
