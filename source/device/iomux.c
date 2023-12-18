

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#include "core/types.h"
#include "core/debug.h"
#include "core/io.h"

#include "device/iomux.h"
#include "device/iodebug.h"



void iomux_init(struct iomux *io, uint base)
{

	for (int n=0; n < IOMUX_NUM_PORTS; n++)
		io_init(&io->port[n]);

	io->base  = base;
	io->chain = NULL;

}



bool iomux_connect(struct iomux *io, u16 port, uint count, struct io ioport)
{

	if (port < io->base || port + count >= io->base + IOMUX_NUM_PORTS)
		return false;

	auto p = &io->port[port - io->base];

	for (int n=0; n < count; n++)
		*p++ = ioport;

	return true;

}



struct iomux *iomux_chain(struct iomux *io, struct iomux *chain)
{

	struct iomux *prev = io->chain;

	io->chain = chain;
	return prev;

}



struct io *iomux_lookup(struct iomux *io, u16 port)
{

	while (port < io->base || port >= io->base + IOMUX_NUM_PORTS) {

		io = io->chain;

		if (io == NULL)
			return NULL;

	}

	return &io->port[port - io->base];

}



void iomux_io_rd(void *data, u16 port, uint mode, uint *value)
{

	struct io *p = iomux_lookup((struct iomux*)data, port);

	if (p != NULL)
		p->rd(p->data, port, mode, value);

	else
		IODEBUG("IOMUX");

}



void iomux_io_wr(void *data, u16 port, uint mode, uint *value)
{

	struct io *p = iomux_lookup((struct iomux*)data, port);

	if (p != NULL)
		p->wr(p->data, port, mode, value);

	else
		IODEBUG("IOMUX");

}

