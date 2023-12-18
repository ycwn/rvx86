

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#include "core/types.h"
#include "core/debug.h"
#include "core/io.h"

#include "device/iobridge.h"



void iobridge_init(struct iobridge *io)
{

	io_init(&io->port);

}



void iobridge_io_rd(struct iobridge *io, u16 port, uint mode, uint *value)
{

	if (IO_RD16(mode)) {

		const uint lo = io_readb(&io->port, port + 0);
		const uint hi = io_readb(&io->port, port + 1);

		*value = hi * 256 + lo;

	} else
		*value = io_readb(&io->port, port);

}



void iobridge_io_wr(struct iobridge *io, u16 port, uint mode, uint *value)
{

	if (IO_WR16(mode)) {

		io_writeb(&io->port, port + 0, (*value >> 0) & 255);
		io_writeb(&io->port, port + 1, (*value >> 8) & 255);

	} else
		io_writeb(&io->port, port, *value);

}

