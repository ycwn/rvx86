

#ifndef CORE_IO_H
#define CORE_IO_H


enum {

	IO_RD8,  IO_WR8,
	IO_RD16, IO_WR16

};


#define IO_RD8(x)   ((x) == IO_RD8)
#define IO_RD16(x)  ((x) == IO_RD16)
#define IO_WR8(x)   ((x) == IO_WR8)
#define IO_WR16(x)  ((x) == IO_WR16)

#define IO_8BIT(x)  (IO_RD8(x)  || IO_WR8(x))
#define IO_16BIT(x) (IO_RD16(x) || IO_WR16(x))
#define IO_RD(x)    (IO_RD8(x)  || IO_RD16(x))
#define IO_WR(x)    (IO_WR8(x)  || IO_WR16(x))


typedef void (io_fn)(void *data, u16 port, uint mode, uint *value);


struct io {

	io_fn *rd;
	io_fn *wr;

	void *data;

};


static void io_nop(void *data, u16 port, uint mode, uint *v) {}


static inline void io_init(struct io *io) {
	io->rd = &io_nop;
	io->wr = &io_nop;
	io->data = NULL;
}

static inline struct io io_make(void *data, io_fn *rd, io_fn *wr) {
	struct io p = { .data=data, .rd=rd, .wr=wr };
	return p;
}

static uint io_readb(struct io *io, u16 port) { uint v=0; io->rd(io->data, port, IO_RD8,  &v); return v; }
static uint io_readw(struct io *io, u16 port) { uint v=0; io->rd(io->data, port, IO_RD16, &v); return v; }

static void io_writeb(struct io *io, u16 port, uint v) { io->wr(io->data, port, IO_WR8,  &v); }
static void io_writew(struct io *io, u16 port, uint v) { io->wr(io->data, port, IO_WR16, &v); }


#endif

