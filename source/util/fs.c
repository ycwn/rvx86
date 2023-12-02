

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#ifdef  HAVE_ZLIB
#include <zlib.h>
#endif

#include "core/types.h"
#include "core/debug.h"

#include "util/fs.h"


static void *fd   = NULL;
static bool  gzip = false;



int fs_load(const char *path, const char *desc, void *buf, size_t blksz, size_t nblks)
{

	printf("Loading %s%s%s...\n",
		(desc != NULL)? desc: "",
		(desc != NULL)? " from ": "",
		path);

	if (!fs_open(path, FS_RD))
		return -1;

	int r = fs_read(buf, blksz, nblks);

	printf("Done.\n");
	fs_close();

	return r;

}



int fs_save(const char *path, const char *desc, const void *buf, size_t blksz, size_t nblks)
{

	printf("Writing %s%s%s...\n",
		(desc != NULL)? desc: "",
		(desc != NULL)? " to ": "",
		path);

	if (!fs_open(path, FS_WR))
		return -1;

	int r = fs_write(buf, blksz, nblks);

	printf("Done.\n");
	fs_close();

	return r;

}



bool fs_open(const char *path, uint mode)
{

	fs_close();

	if (mode == FS_RD) {

#if HAVE_ZLIB
		fd   = gzopen(path, "rb");
		gzip = true;
#else
		fd   = fopen(path, "rb");
		gzip = false;
#endif

	} else if (mode == FS_WR) {

		int len = strlen(path);

		if (len >= 3 && !strcmp(path + len - 3, ".gz")) {

#if HAVE_ZLIB
			fd   = gzopen(path, "wb9");
			gzip = true;
#else
			return false;
#endif

		} else {

			fd   = fopen(path, "wb");
			gzip = false;

		}

	} else
		return false;

	if (fd == NULL) {

		perror("fopen()");
		return false;

	}

	return true;

}



void fs_close()
{

	if (fd != NULL) {

		if (gzip) {
#if HAVE_ZLIB
			gzclose((gzFile)fd);
#endif
		} else
			fclose((FILE*)fd);

	}

	fd   = NULL;
	gzip = false;

}



int fs_read(void *buf, size_t blksz, size_t nblks)
{

	if (fd != NULL) {

		if (gzip) {
#if HAVE_ZLIB
			return gzfread(buf, blksz, nblks, (gzFile)fd);
#endif
		} else
			return fread(buf, blksz, nblks, (FILE*)fd);

	}

	return 0;

}



int fs_write(const void *buf, size_t blksz, size_t nblks)
{

	if (fd != NULL) {

		if (gzip) {
#if HAVE_ZLIB
			return gzfwrite(buf, blksz, nblks, (gzFile)fd);
#endif
		} else
			return fwrite(buf, blksz, nblks, (FILE*)fd);


	}

	return 0;

}



char *fs_gets()
{

	static char buf[256];


	if (fd != NULL) {

		if (gzip) {
#if HAVE_ZLIB
			return gzgets((gzFile)fd, buf, sizeof(buf));
#endif
		} else
			return fgets(buf, sizeof(buf), (FILE*)fd);

	}

	return NULL;

}

