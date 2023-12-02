

#ifndef UTIL_TRIM_H
#define UTIL_TRIM_H


static char *ltrim(char *s) { while(isspace(*s)) s++; return s; }
static char *rtrim(char *s) { char *back = s + strlen(s); while (isspace(*--back)); *(back+1) = '\0'; return s; }
static char *trim(char *s)  { return rtrim(ltrim(s)); }


#endif

