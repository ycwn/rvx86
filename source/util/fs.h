

#ifndef UTIL_FS_H
#define UTIL_FS_H


enum {
	FS_RD = 0,
	FS_WR = 1
};


int fs_load(const char *path, const char *desc, void *buf, size_t blksz, size_t nblks);
int fs_save(const char *path, const char *desc, const void *buf, size_t blksz, size_t nblks);

bool  fs_open(const char *path, uint mode);
void  fs_close();
int   fs_read(void *buf, size_t len, size_t count);
int   fs_write(const void *buf, size_t len, size_t count);
char *fs_gets();


#endif

