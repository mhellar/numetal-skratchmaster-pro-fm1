/* the one libc call SAM needs that platform's libc.c does not give */
unsigned strlen(const char *s) { const char *p = s; while (*p) p++; return (unsigned)(p - s); }
