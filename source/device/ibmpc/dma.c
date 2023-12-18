

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "core/types.h"
#include "core/debug.h"
#include "core/io.h"
#include "core/memory.h"
#include "core/wire.h"

#include "device/ibmpc/dma.h"



void dma_init(DMA *dma)
{

	memory_init(&dma->memory);

	dma_reset(dma);

	for (int n=0; n < DMA_NUM_CHANNELS; n++)
		dma_channel_set_page(dma, n, 0);

}



void dma_reset(DMA *dma)
{

	dma_clr_hilo(dma);

	for (int n=0; n < DMA_NUM_CHANNELS; n++)
		dma_channel_reset(dma, n);

}



void dma_clr_hilo(DMA *dma)
{

	dma->hilo = false;

}



void dma_set_mask(DMA *dma, uint mask)
{

	for (int n=0; n < DMA_NUM_CHANNELS; n++)
		dma_channel_enable(dma, n, (mask & 1 << n) == 0);

}



uint dma_status(DMA *dma)
{

	uint status = 0;

	for (int n=0; n < DMA_NUM_CHANNELS; n++) {

		auto dc = &dma->channel[n];

		if (dc->complete)
			status |= 1 << n;

		dc->complete = false;

	}

	return status;

}



void dma_channel_reset(DMA *dma, uint ch)
{

	if (ch >= DMA_NUM_CHANNELS)
		return;

	auto dc = &dma->channel[ch];

	dc->base_addr[0] = 0;
	dc->base_addr[1] = 0;

	dc->base_size[0] = 0;
	dc->base_size[1] = 0;

	dc->curr_addr = 0;
	dc->curr_size = 0;

	dc->autoload = false;
	dc->complete = false;
	dc->enabled  = false;
	dc->memread  = false;
	dc->memwrite = false;

	dc->direction = 1;

}



void dma_channel_config(DMA *dma, uint ch, uint mode)
{

	if (ch >= DMA_NUM_CHANNELS)
		return;

	auto dc = &dma->channel[ch];

	dc->autoload  = (mode & 0x10) != 0;
	dc->complete  = false;
	dc->memread   = (mode & 0x08) != 0;
	dc->memwrite  = (mode & 0x04) != 0;
	dc->direction = (mode & 0x20)? -1: +1;

}



void dma_channel_enable(DMA *dma, uint ch, bool enable)
{

	if (ch >= DMA_NUM_CHANNELS)
		return;

	dma->channel[ch].enabled = enable;

}



void dma_channel_reload(DMA *dma, uint ch)
{

	if (ch >= DMA_NUM_CHANNELS)
		return;

	auto dc = &dma->channel[ch];

	dc->curr_addr = dc->base_addr[1] * 256 + dc->base_addr[0];
	dc->curr_size = dc->base_size[1] * 256 + dc->base_size[0];
	dc->complete  = false;

}



uint dma_channel_get_addr(DMA *dma, uint ch)
{

	if (ch >= DMA_NUM_CHANNELS)
		return 0;

	dma->hilo = !dma->hilo;

	return dma->channel[ch].base_addr[!dma->hilo];

}



uint dma_channel_get_page(DMA *dma, uint ch)
{

	if (ch >= DMA_NUM_CHANNELS)
		return 0;

	return dma->channel[ch].page;

}



uint dma_channel_get_size(DMA *dma, uint ch)
{

	if (ch >= DMA_NUM_CHANNELS)
		return 0;

	dma->hilo = !dma->hilo;

	return dma->channel[ch].base_size[!dma->hilo];

}



void dma_channel_set_addr(DMA *dma, uint ch, u8 addr)
{

	if (ch >= DMA_NUM_CHANNELS)
		return;

	auto dc = &dma->channel[ch];

	dc->base_addr[dma->hilo] = addr;

	if (dma->hilo)
		dc->curr_addr = dc->base_addr[1] * 256 + dc->base_addr[0];

	dma->hilo = !dma->hilo;

}



void dma_channel_set_page(DMA *dma, uint ch, u8 page)
{

	if (ch >= DMA_NUM_CHANNELS)
		return;

	dma->channel[ch].page = page;

}



void dma_channel_set_size(DMA *dma, uint ch, u8 val)
{

	if (ch >= DMA_NUM_CHANNELS)
		return;

	auto dc = &dma->channel[ch];

	dc->base_size[dma->hilo] = val;

	if (dma->hilo)
		dc->curr_size = dc->base_size[1] * 256 + dc->base_size[0];

	dma->hilo = !dma->hilo;

}



int dma_channel_request(DMA *dma, uint ch, uint data)
{

	if (ch >= DMA_NUM_CHANNELS)
		return -1;

	auto dc = &dma->channel[ch];

	if (!dc->enabled) // Check channel mask
		return -1;

	if (dc->complete) {

		if (!dc->autoload)
			return -1;

		dma_channel_reload(dma, ch);

	}

	const u32 addr = (dc->page * 65536 + dc->curr_addr) & dma->memory.mask;

	if (dc->memwrite) dma->memory.base[addr] = data;
	if (dc->memread)  data = dma->memory.base[addr];

	dc->curr_addr += dc->direction;

	if (dc->curr_size-- == 0) // TC reached
		dc->complete = true;

	return data;

}



void dma_io_rd(DMA *dma, u16 port, uint mode, uint *value)
{

	switch (port) {

		// Read curr address
		case 0x00: *value = dma_channel_get_addr(dma, 0); break;
		case 0x02: *value = dma_channel_get_addr(dma, 1); break;
		case 0x04: *value = dma_channel_get_addr(dma, 2); break;
		case 0x06: *value = dma_channel_get_addr(dma, 3); break;

		// Read curr count
		case 0x01: *value = dma_channel_get_size(dma, 0); break;
		case 0x03: *value = dma_channel_get_size(dma, 1); break;
		case 0x05: *value = dma_channel_get_size(dma, 2); break;
		case 0x07: *value = dma_channel_get_size(dma, 3); break;

		// Read status
		case 0x08: *value = dma_status(dma); break;

		case 0x81: *value = dma_channel_get_page(dma, 2); break;  // Read CH2 page
		case 0x82: *value = dma_channel_get_page(dma, 3); break;  // Read CH3 page
		case 0x83: *value = dma_channel_get_page(dma, 1); break;  // Read CH1 page

	}

	//IODEBUG("DMA");

}



void dma_io_wr(DMA *dma, u16 port, uint mode, uint *value)
{

	switch (port) {

		// Write base curr address
		case 0x00: dma_channel_set_addr(dma, 0, *value); break;
		case 0x02: dma_channel_set_addr(dma, 1, *value); break;
		case 0x04: dma_channel_set_addr(dma, 2, *value); break;
		case 0x06: dma_channel_set_addr(dma, 3, *value); break;

		// Write base curr count
		case 0x01: dma_channel_set_size(dma, 0, *value); break;
		case 0x03: dma_channel_set_size(dma, 1, *value); break;
		case 0x05: dma_channel_set_size(dma, 2, *value); break;
		case 0x07: dma_channel_set_size(dma, 3, *value); break;

		case 0x08: break;  // Command register unused
 		case 0x09: break;  // Request register unused

		case 0x0a: dma_channel_enable(dma, *value & 0x03, (*value & 0x04) == 0); break;  // Set channel mask
		case 0x0b: dma_channel_config(dma, *value & 0x03, *value);               break;  // Set channel mode

		case 0x0c: dma_clr_hilo(dma);         break;  // Clear high/low flip-flop
		case 0x0d: dma_reset(dma);            break;  // Reset
		case 0x0e: dma_set_mask(dma, ~0);     break;  // Clear mask
		case 0x0f: dma_set_mask(dma, *value); break;  // Write mask

		case 0x81: dma_channel_set_page(dma, 2, *value); break;  // Set CH2 page
		case 0x82: dma_channel_set_page(dma, 3, *value); break;  // Set CH3 page
		case 0x83: dma_channel_set_page(dma, 1, *value); break;  // Set CH1 page

	}

	//IODEBUG("DMA");

}

