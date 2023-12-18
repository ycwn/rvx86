

#ifndef DEVICE_IOMUX_H
#define DEVICE_IOMUX_H


enum {
	IOMUX_NUM_PORTS = 1024
};


struct iomux {

	uint base;

	struct iomux *chain;
	struct io     port[IOMUX_NUM_PORTS];

};


void iomux_init(struct iomux *io, uint base);

bool          iomux_connect(struct iomux *io, u16 port, uint count, struct io ioport);
struct iomux *iomux_chain(  struct iomux *io, struct iomux *chain);
struct io    *iomux_lookup( struct iomux *io, u16 port);

void iomux_io_rd(void *data, u16 port, uint mode, uint *value);
void iomux_io_wr(void *data, u16 port, uint mode, uint *value);


static inline struct io iomux_mkport(struct iomux *io) {
	return io_make(io, &iomux_io_rd, &iomux_io_wr);
}



#endif

