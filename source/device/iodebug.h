

#ifndef DEVICE_IODEBUG_H
#define DEVICE_IODEBUG_H


#define IODEBUG(X)                                                        \
	do {                                                              \
		if (IO_RD(mode)) printf(X " RD %x: %x\n", port, *value);  \
		else             printf(X " WR %x: %x\n", port, *value);  \
	} while (0)


#endif

