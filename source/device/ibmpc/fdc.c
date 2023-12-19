

#include <stddef.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "core/types.h"
#include "core/debug.h"
#include "core/io.h"
#include "core/wire.h"

#include "device/ibmpc/fdc.h"
#include "device/iodebug.h"

#include "util/fs.h"


static void transfer_dma_byte(FDC *fdc, int drive, bool write);

static void cmd_undef(  FDC *fdc);
static void cmd_specify(FDC *fdc);
static void cmd_datawr( FDC *fdc);
static void cmd_datard( FDC *fdc);
static void cmd_recalb( FDC *fdc);
static void cmd_sense(  FDC *fdc);
static void cmd_format( FDC *fdc);
static void cmd_seek(   FDC *fdc);


typedef void (*command)(FDC *fdc);

static const command commands[16] = {
	&cmd_undef, &cmd_undef,  &cmd_undef,  &cmd_specify,
	&cmd_undef, &cmd_datawr, &cmd_datard, &cmd_recalb,
	&cmd_sense, &cmd_undef,  &cmd_undef,  &cmd_undef,
	&cmd_undef, &cmd_format, &cmd_undef,  &cmd_seek
};



void fdc_init(FDC *fdc)
{

	fdc_reset(fdc);

	for (int n=0; n < FLOPPY_NUM_DRIVES; n++) {

		fdc_set_type(fdc, n, 1440);

		fdc->drive[n].sector = 0;
		fdc->drive[n].track  = 0;
		fdc->drive[n].head   = 0;

	}

	wire_init(&fdc->irq);
	wire_init(&fdc->dma);

}



void fdc_tick(FDC *fdc)
{

	if (fdc->delay > 0) {
		
		fdc->delay--;
		return;

	}

	if (fdc->argc > 0)
		commands[fdc->argv[0] & 15](fdc);

}



void fdc_reset(FDC *fdc)
{

	fdc->resn = 0;
	fdc->resc = 0;
	fdc->argc = 0;

	fdc->busy  = false;
	fdc->delay = 0;

}



uint fdc_status(FDC *fdc)
{

	return fdc->busy? 0x10: (fdc->resn > 0)? 0xc0: 0x80;

}



uint fdc_read(FDC *fdc)
{

	return (fdc->resc < fdc->resn)? fdc->resv[fdc->resc++]: 0;

}



void fdc_write(FDC *fdc, uint val)
{

	if (!fdc->busy)
		fdc->argv[fdc->argc++] = val;

}



int fdc_set_type(FDC *fdc, int drive, int sectors)
{

	if (drive < 0 || drive >= FLOPPY_NUM_DRIVES)
		return -1;

	auto drv = &fdc->drive[drive];

	switch (sectors) {

		// 5¼ Floppy Disk formats
		case 2400: drv->heads = 2; drv->tracks = 80; drv->sectors = 15; break;  // 5¼ 1200kB: H=2 T=80 S=15
		case 1280: drv->heads = 2; drv->tracks = 80; drv->sectors = 8;  break;  // 5¼ 640kB:  H=2 T=80 S=8
		case  640: drv->heads = 2; drv->tracks = 40; drv->sectors = 8;  break;  // 5¼ 320kB:  H=2 T=40 S=8
		case  720: drv->heads = 2; drv->tracks = 40; drv->sectors = 9;  break;  // 5¼ 360kB:  H=2 T=40 S=9
		case  360: drv->heads = 1; drv->tracks = 40; drv->sectors = 9;  break;  // 5¼ 180kB:  H=1 T=40 S=9
		case  320: drv->heads = 1; drv->tracks = 40; drv->sectors = 8;  break;  // 5¼ 160kB:  H=1 T=40 S=8

		// 3½ Floppy Disk formats
		case 5760: drv->heads = 2; drv->tracks = 80; drv->sectors = 36; break;  // 3½ 2880kB: H=2 T=80 S=36
		case 2880: drv->heads = 2; drv->tracks = 80; drv->sectors = 18; break;  // 3½ 1440kB: H=2 T=80 S=18
		case 1440: drv->heads = 2; drv->tracks = 80; drv->sectors = 9;  break;  // 3½ 720kB:  H=2 T=80 S=9

		default:
			return -1;

	}

	return 0;

}



int fdc_get_sectors(FDC *fdc, int drive)
{

	if (drive < 0 || drive >= FLOPPY_NUM_DRIVES)
		return -1;

	auto drv = &fdc->drive[drive];

	return drv->heads * drv->tracks * drv->sectors;

}



const char *fdc_describe(FDC *fdc, int drive)
{

	static char buf[256];

	if (drive < 0 || drive >= FLOPPY_NUM_DRIVES)
		return NULL;

	auto drv = &fdc->drive[drive];

	const char *dims = (drv->tracks == 40 || drv->sectors == 8 || drv->sectors == 15)? "5¼": "3½";
	const uint  size = drv->heads * drv->tracks * drv->sectors / 2;

	sprintf(buf, "%s %dkB: %d sides, %d tracks, %d sectors",
		dims, size, drv->heads, drv->tracks, drv->sectors);

	return buf;

}



int fdc_load(FDC *fdc, int drive, const char *file)
{

	if (drive < 0 || drive >= FLOPPY_NUM_DRIVES)
		return -1;

	int sectors = fs_load(file, "floppy image", fdc->drive[drive].disk, 512, 2880);

	return fdc_set_type(fdc, drive, sectors);

}



int fdc_save(FDC *fdc, int drive, const char *file)
{

	int sectors = fdc_get_sectors(fdc, drive);

	if (sectors < 0)
		return -1;

	return fs_save(file, "floppy image", fdc->drive[drive].disk, 512, sectors);

}



void fdc_io_rd(FDC *fdc, u16 port, uint mode, uint *value)
{

	switch (port) {

		case 0x3f4: *value = fdc_status(fdc); break;
		case 0x3f5: *value = fdc_read(fdc);   break;

	}

	//IODEBUG("FDC");

}



void fdc_io_wr(FDC *fdc, u16 port, uint mode, uint *value)
{

	switch (port) {

		case 0x3f2:
			if (*value & 0x04) wire_act(&fdc->irq); // FDC raises IRQ when reset
			else               fdc_reset(fdc);
			break;

		case 0x3f5:
			fdc_write(fdc, *value);
			break;

	}

	//IODEBUG("FDC");

}



void transfer_dma_byte(FDC *fdc, int drvno, bool write)
{

	auto drv  = &fdc->drive[drvno];
	uint lba  = ((drv->track * drv->heads + drv->head) * drv->sectors) + drv->sector - 1;
	bool done = false;
	int  data;

	if (write) {

		data = wire_actv(&fdc->dma, 0);

		if (data >= 0)
			drv->disk[lba * 512 + drv->byteno] = data;

		else
			done = true;

	} else {

		data = drv->disk[lba * 512 + drv->byteno];

		if (wire_actv(&fdc->dma, data) < 0)
			done = true;

	}

	// Advance the head
	if (++drv->byteno >= 512) {

		drv->byteno = 0;

		if (++drv->sector > drv->sectors) {

			drv->sector = 1;

			if (++drv->head >= drv->heads)
				drv->head = 0;

		}

	}

	// The FDC doesn't know how much data to transfer
	// It stops on NAK from the DMA
	if (done) {

		fdc->resv[0] = 0x20;
		fdc->resv[1] = 0x00;
		fdc->resv[2] = 0x00;
		fdc->resv[3] = drv->track;
		fdc->resv[4] = drv->head;
		fdc->resv[5] = drv->sector;
		fdc->resv[6] = 0x02;
		fdc->resn    = 7;
		fdc->busy    = false;

		wire_act(&fdc->irq);

	}

}



void cmd_undef(FDC *fdc)
{

	watch("%d", fdc->argc);
	watch("%x", fdc->argv[0]);
	watch("%x", fdc->argv[1]);
	watch("%x", fdc->argv[2]);
	watch("%x", fdc->argv[3]);

	abort();

}



void cmd_specify(FDC *fdc)
{

	if (fdc->argc < 3)
		return;

	fdc_reset(fdc);

}



void cmd_datawr(FDC *fdc)
{

	if (fdc->argc < 9)
		return;

	int  drvno = fdc->argv[1] & 3;
	auto drv   = &fdc->drive[drvno];

	if (fdc->resn > 0) {

		if (fdc->resc >= fdc->resn)
			fdc_reset(fdc);

	} else if (!fdc->busy) {

		drv->track  = fdc->argv[2];
		drv->head   = fdc->argv[3];
		drv->sector = ((fdc->argv[0] & 0x0f) == 2)? 1: fdc->argv[4];
		drv->byteno = 0;

		fdc->busy = true;

	} else
		transfer_dma_byte(fdc, drvno, true);

}



void cmd_datard(FDC *fdc)
{

	if (fdc->argc < 9)
		return;

	int  drvno = fdc->argv[1] & 3;
	auto drv   = &fdc->drive[drvno];

	if (fdc->resn > 0) {

		if (fdc->resc >= fdc->resn)
			fdc_reset(fdc);

	} else if (!fdc->busy) {

		drv->track  = fdc->argv[2];
		drv->head   = fdc->argv[3];
		drv->sector = ((fdc->argv[0] & 0x0f) == 2)? 1: fdc->argv[4];
		drv->byteno = 0;

		fdc->busy = true;

	} else
		transfer_dma_byte(fdc, drvno, false);

}



void cmd_recalb(FDC *fdc)
{

	if (fdc->argc < 2)
		return;

	if (fdc->busy) {

		fdc_reset(fdc);
		wire_act(&fdc->irq);

	} else {

		fdc->busy  = true;
		fdc->delay = 100;

		fdc->drive[fdc->argv[1] & 3].track = 0;

	}

}



void cmd_sense(FDC *fdc)
{

	if (fdc->resn == 0) {

		fdc->resv[0] = 0x20;//fdc->error? 0x80: 0x20;
		fdc->resv[1] = 0x00;
		fdc->resn    = 2;

	}

	if (fdc->resc >= fdc->resn)
		fdc_reset(fdc);

}



void cmd_format(FDC *fdc)
{

	if (fdc->argc < 6)
		return;

	int  drvno = fdc->argv[1] & 3;
	auto drv   = &fdc->drive[drvno];

	if (fdc->resn > 0) {

		if (fdc->resc >= fdc->resn)
			fdc_reset(fdc);

	} else if (!fdc->busy) {

		drv->byteno = 0;
		fdc->busy   = true;

	} else {

		int data = wire_actv(&fdc->dma, 0);

		if (data < 0) {

			fdc->resv[0] = 0x20;
			fdc->resv[1] = 0x00;
			fdc->resv[2] = 0x00;
			fdc->resv[3] = drv->track;
			fdc->resv[4] = drv->head;
			fdc->resv[5] = drv->sector;
			fdc->resv[6] = 0x02;
			fdc->resn    = 7;
			fdc->busy    = false;

			wire_act(&fdc->irq);
			return;

		}

		switch (drv->byteno) {

			case 0: drv->byteno = 1; drv->track  = data; return;
			case 1: drv->byteno = 2; drv->head   = data; return;
			case 2: drv->byteno = 3; drv->sector = data; return;
			case 3: drv->byteno = 0;                     break;

		}

		if (drv->track >= drv->tracks || drv->head >= drv->heads || drv->sector > drv->sectors)
			return;

		uint lba = ((drv->track * drv->heads + drv->head) * drv->sectors) + drv->sector - 1;
		memset(&drv->disk[lba * 512], fdc->argv[5], 512);

	}

}



void cmd_seek(FDC *fdc)
{

	if (fdc->argc < 3)
		return;

	if (fdc->busy) {

		fdc_reset(fdc);
		wire_act(&fdc->irq);

	} else { //FIXME: Check if track is valid and set error flag

		fdc->busy  = true;
		fdc->delay = 100;

		fdc->drive[fdc->argv[1] & 3].track = fdc->argv[2];

	}

}

