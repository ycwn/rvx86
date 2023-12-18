

#ifndef DEVICE_DEVICE_H
#define DEVICE_DEVICE_H


struct iobridge {

	struct io port;

};


void iobridge_init(struct iobridge *io);

void iobridge_io_rd(struct iobridge *io, u16 port, uint mode, uint *value);
void iobridge_io_wr(struct iobridge *io, u16 port, uint mode, uint *value);


static inline struct io iobridge_mkport(struct iobridge *io) {
	return io_make(io, (io_fn*)&iobridge_io_rd, (io_fn*)&iobridge_io_wr);
}


#endif

