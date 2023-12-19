

#ifndef DEVICE_FLOPPY_H
#define DEVICE_FLOPPY_H


enum {

	FLOPPY_NUM_DRIVES  = 2,
	FLOPPY_NUM_HEADS   = 2,
	FLOPPY_NUM_TRACKS  = 80,
	FLOPPY_NUM_SECTORS = 18,

	FLOPPY_SECTOR_SIZE  = 512,
	FLOPPY_SECTOR_TOTAL = FLOPPY_NUM_HEADS * FLOPPY_NUM_TRACKS * FLOPPY_NUM_SECTORS

};


typedef struct {

	int argc;
	int argv[16];

	int resc;
	int resn;
	int resv[16];

	bool busy;
	uint delay;

	struct {

		u8 disk[FLOPPY_SECTOR_SIZE * FLOPPY_SECTOR_TOTAL];

		uint heads;
		uint sectors;
		uint tracks;

		uint byteno;
		uint sector;
		uint track;
		uint head;

	} drive[FLOPPY_NUM_DRIVES];

	struct wire irq;
	struct wire dma;

} FDC;


void fdc_init(  FDC *fdc);
void fdc_tick(  FDC *fdc);
void fdc_reset( FDC *fdc);
uint fdc_status(FDC *fdc);
uint fdc_read(  FDC *fdc);
void fdc_write( FDC *fdc, uint val);

int fdc_set_type(FDC *fdc, int drive, int sectors);
int fdc_get_sectors(FDC *fdc, int drive);

const char *fdc_describe(FDC *fdc, int drive);

int fdc_load(FDC *fdc, int drive, const char *file);
int fdc_save(FDC *fdc, int drive, const char *file);

void fdc_io_rd(FDC *fdc, u16 port, uint mode, uint *value);
void fdc_io_wr(FDC *fdc, u16 port, uint mode, uint *value);


static inline struct io fdc_mkport(FDC *fdc) {
	return io_make(fdc, (io_fn*)&fdc_io_rd, (io_fn*)&fdc_io_wr);
}


#endif

