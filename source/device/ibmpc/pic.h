

#ifndef DEVICE_PIC_H
#define DEVICE_PIC_H


typedef struct {

	struct {

		bool icw4;
		bool cascade;

		uint address;
		uint level;

	} config;


	struct {

		bool poll_mode;
		bool read_isr;

	} operation;


	struct {

		bool raised[8];
		bool masked[8];
		int  active;

	} interrupt;


	struct wire intrq;

} PIC;


void pic_init(   PIC *pic);
void pic_reset(  PIC *pic);
void pic_config( PIC *pic, uint value);
void pic_command(PIC *pic, uint cmd);
void pic_update( PIC *pic);

uint pic_get_raised(PIC *pic);
uint pic_get_active(PIC *pic);
uint pic_get_mask(PIC *pic);
void pic_set_mask(PIC *pic, uint mask);

int pic_irq_clear(PIC *pic, int  irq);
int pic_irq_raise(PIC *pic, uint irq, uint arg);

void pic_io_rd(PIC *pic, u16 port, uint mode, uint *value);
void pic_io_wr(PIC *pic, u16 port, uint mode, uint *value);


static inline struct io pic_mkport(PIC *pic) {
	return io_make(pic, (io_fn*)&pic_io_rd, (io_fn*)&pic_io_wr);
}

static inline struct wire pic_mkirq(PIC *pic, uint irq) {
	return wire_make((wire_fn*)&pic_irq_raise, pic, irq);
}


#endif
