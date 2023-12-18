

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#include "core/types.h"
#include "core/debug.h"
#include "core/io.h"
#include "core/wire.h"

#include "device/ibmpc/pic.h"
#include "device/iodebug.h"



void pic_init(PIC *pic)
{

	wire_init(&pic->intrq);
	pic_reset(pic);

}



void pic_reset(PIC *pic)
{

	pic->config.icw4    = false;
	pic->config.cascade = false;
	pic->config.address = 0;
	pic->config.level   = 4;

	pic->operation.poll_mode = false;
	pic->operation.read_isr  = false;

	for (int n=0; n < 8; n++) {

		pic->interrupt.raised[n] = false;
		pic->interrupt.masked[n] = false;

	}

	pic->interrupt.active = -1;

}



void pic_config(PIC *pic, uint value)
{

	switch (pic->config.level) {

		// ICW1
		case 0:
			pic->config.icw4     = (value & 0x01) != 0;
			pic->config.cascade  = (value & 0x02) == 0;
			pic->config.level    = 1;
			break;

		// ICW2
		case 1:
			pic->config.address = value & 0xf8;
			pic->config.level   = 2;
			break;

		// ICW3
		case 2:
			pic->config.level = 3;
			break;

		// ICW4
		case 3:
			pic->config.level   = 4;
			break;

	}

	// Skip ICW3 if in single mode
	if (pic->config.level == 2 && !pic->config.cascade)
		pic->config.level = 3;

	// Skip ICW4 if not required
	if (pic->config.level == 3 && !pic->config.icw4)
		pic->config.level = 4;

}



void pic_command(PIC *pic, uint cmd)
{

	const bool cmd_init = (cmd & 0x10) != 0;
	const bool cmd_ocw2 = (cmd & 0x08) == 0;

	if (cmd_init) { // ICW1

		pic_reset(pic);

		pic->config.level = 0;
		pic_config(pic, cmd);

	} else if (cmd_ocw2) { // OCW2

		const bool eoi      = (cmd & 0x20) != 0;
		const bool specific = (cmd & 0x40) != 0;
		const uint level    = (cmd & 0x07);

		if (eoi)
			pic_irq_clear(pic, specific? level: -1);

	} else { // OCW3

		pic->operation.poll_mode = (cmd & 0x04) != 0;
		pic->operation.read_isr  = (cmd & 0x03) == 3;

	}

}



void pic_update(PIC *pic)
{

	const int num = (pic->interrupt.active >= 0)? pic->interrupt.active: 8;

	for (int irq=0; irq < num; irq++)
		if (pic->interrupt.raised[irq]) {

			pic->interrupt.active = irq;
			wire_actv(&pic->intrq, pic->config.address + irq);
			break;

		}

}



uint pic_get_raised(PIC *pic)
{

	uint irqs = 0;

	for (int n=0; n < 8; n++)
		irqs |= pic->interrupt.raised[n] << n;

	return irqs;

}



uint pic_get_active(PIC *pic)
{

	return (pic->interrupt.active >= 0)? (1 << pic->interrupt.active): 0;

}



uint pic_get_mask(PIC *pic)
{

	uint irqs = 0;

	for (int n=0; n < 8; n++)
		irqs |= pic->interrupt.masked[n] << n;

	return irqs;

}



void pic_set_mask(PIC *pic, uint mask)
{

	for (int n=0; n < 8; n++)
		pic->interrupt.masked[n] = (mask & (1 << n)) != 0;

}



int pic_irq_clear(PIC *pic, int irq)
{

	if (irq < 0)
		irq = pic->interrupt.active;

	if (irq >= 0) {

		pic->interrupt.raised[irq] = false;
		pic->interrupt.active      = -1;

	}

	pic_update(pic);

	return irq;

}



int pic_irq_raise(PIC *pic, uint irq, uint arg)
{

	if (irq >= 8 || pic->interrupt.masked[irq])
		return -1;

	pic->interrupt.raised[irq] = true;
	pic_update(pic);

	return 0;

}



void pic_io_rd(PIC *pic, u16 port, uint mode, uint *value)
{

	const bool data_port = (port & 0x01) != 0;

	*value = data_port?                pic_get_mask(pic):
	         pic->operation.poll_mode? pic_irq_clear(pic, -1):
                 pic->operation.read_isr?  pic_get_raised(pic): pic_get_active(pic);

	//IODEBUG("PIC");

}



void pic_io_wr(PIC *pic, u16 port, uint mode, uint *value)
{

	const bool data_port = (port & 0x01) != 0;
	const bool init_done = pic->config.level >= 4;

	if (data_port) {

		// Configuration complete, process commands
		if (init_done) pic_set_mask(pic, *value);
		else           pic_config(  pic, *value);

	} else
		pic_command(pic, *value);

	//IODEBUG("PIC");

}

