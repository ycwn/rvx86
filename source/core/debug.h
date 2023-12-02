

#ifndef CORE_DEBUG_H
#define CORE_DEBUG_H


#undef checkpoint
#undef assert
#undef value


#ifdef DEBUG

#include <stdio.h>

#define checkpoint() \
	do { \
		printf("%s, %d: check!\n", __FILE__, __LINE__); \
	} while (0)

#define assert(pred) \
	do { \
		if (!(pred)) \
			printf("%s, %d: assertion '%s' failed!\n", __FILE__, __LINE__, #pred); \
	} while (0)

#define watch(format, var...) \
	do { \
		printf("%s, %d: %s = " format "\n", __FILE__, __LINE__, #var, var); \
	} while(0)

#define log_d(format, ...) do { printf(format, ## __VA_ARGS__); } while (0)
#define log_i(format, ...) do { printf(format, ## __VA_ARGS__); } while (0)
#define log_w(format, ...) do { printf(format, ## __VA_ARGS__); } while (0)
#define log_e(format, ...) do { printf(format, ## __VA_ARGS__); } while (0)
#define log_c(format, ...) do { printf(format, ## __VA_ARGS__); } while (0)

#else

#define checkpoint() { }    do { /* Nothing */ } while (0)
#define assert(pred) { }    do { /* Nothing */ } while (0)
#define watch(format, ...)  do { /* Nothing */ } while (0)
#define log(format, ...)    do { /* Nothing */ } while (0)

#endif


#endif

