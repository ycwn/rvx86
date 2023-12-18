

#ifndef DEVICE_DMA_H
#define DEVICE_DMA_H


enum {

	DMA_NUM_CHANNELS = 4

};


typedef struct {

	struct memory memory;
	bool          hilo;


	struct {

		uint base_addr[2];
		uint base_size[2];

		uint curr_addr;
		uint curr_size;
		uint page;

		bool autoload;
		bool complete;
		bool enabled;
		bool memread;
		bool memwrite;

		int direction;

	} channel[DMA_NUM_CHANNELS];

} DMA;


void dma_init(    DMA *dma);
void dma_reset(   DMA *dma);
void dma_clr_hilo(DMA *dma);
void dma_set_mask(DMA *dma, uint mask);
uint dma_status(  DMA *dma);

void dma_channel_reset(   DMA *dma, uint ch);
void dma_channel_config(  DMA *dma, uint ch, uint mode);
void dma_channel_enable(  DMA *dma, uint ch, bool enable);
void dma_channel_reload(  DMA *dma, uint ch);
uint dma_channel_get_addr(DMA *dma, uint ch);
uint dma_channel_get_page(DMA *dma, uint ch);
uint dma_channel_get_size(DMA *dma, uint ch);
void dma_channel_set_addr(DMA *dma, uint ch, u8 val);
void dma_channel_set_page(DMA *dma, uint ch, u8 val);
void dma_channel_set_size(DMA *dma, uint ch, u8 val);
int  dma_channel_request( DMA *dma, uint ch, uint data);

void dma_io_rd(DMA *dma, u16 port, uint mode, uint *value);
void dma_io_wr(DMA *dma, u16 port, uint mode, uint *value);


static inline struct io dma_mkport(DMA *dma) {
	return io_make(dma, (io_fn*)&dma_io_rd, (io_fn*)&dma_io_wr);
}

static inline struct wire dma_mkdrq(DMA *dma, uint ch) {
	return wire_make((wire_fn*)&dma_channel_request, dma, ch);
}


#endif

