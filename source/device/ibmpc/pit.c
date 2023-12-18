

#include <stddef.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>

#include "core/types.h"
#include "core/debug.h"
#include "core/io.h"
#include "core/wire.h"

#include "device/ibmpc/pit.h"
#include "device/iodebug.h"

#include "util/bcd.h"



void pit_init(PIT *pit)
{

	auto tc = &pit->channel[0];

	for (int n=0; n < PIT_NUM_CHANNELS; n++, tc++) {

		wire_init(&tc->config);
		wire_init(&tc->output);

		pit_channel_reset(pit, n);

	}

}



void pit_command(PIT *pit, uint cmd)
{

	const uint ch = (cmd >> 6) & 3;

	if (ch < PIT_NUM_CHANNELS)
		return pit_channel_config(pit, ch, cmd);

// Readback status only, latch ignored
	pit->channel[0].status = (cmd & 0x12) == 0x02;
	pit->channel[1].status = (cmd & 0x14) == 0x04;
	pit->channel[2].status = (cmd & 0x18) == 0x08;

}



void pit_channel_reset(PIT *pit, uint ch)
{

	if (ch >= PIT_NUM_CHANNELS)
		return;

	auto tc = &pit->channel[ch];

	tc->count[0] = 0;
	tc->count[1] = 0;

	tc->cfg    = 0;
	tc->period = 0;
	tc->freq   = 0;

	tc->enabled = false;
	tc->bcd     = false;
	tc->hilo    = false;
	tc->flip    = false;
	tc->status  = false;

}



void pit_channel_config(PIT *pit, uint ch, uint cfg)
{

	const uint acc = (cfg >> 4) & 3;
	const uint mod = (cfg >> 1) & 7;
	const uint bcd = (cfg >> 0) & 1;

	if (ch >= PIT_NUM_CHANNELS || acc == 0) // Ignore latch command (acc==0)
		return;

	pit_channel_reset(pit, ch);

	auto tc = &pit->channel[ch];

	tc->cfg  = cfg;
	tc->bcd  = bcd;
	tc->hilo = acc == 2;
	tc->flip = acc == 3;

}



uint pit_channel_read(PIT *pit, uint ch)
{

	if (ch >= PIT_NUM_CHANNELS)
		return 0;

	auto tc = &pit->channel[ch];

	if (tc->status) {

		tc->status = false;
		return pit_channel_status(pit, ch);

	}

	uint val = tc->count[tc->hilo];
	tc->hilo = tc->hilo ^ tc->flip;

	return val;

}



void pit_channel_write(PIT *pit, uint ch, uint val)
{

	if (ch >= PIT_NUM_CHANNELS)
		return;

	auto tc = &pit->channel[ch];

	tc->count[tc->hilo] = val;

	tc->hilo = tc->hilo ^ tc->flip;

	uint count = tc->count[1] * 256 + tc->count[0];

	if (tc->bcd)
		count = bcd2dec16(count);

	if (count == 0)
		count = tc->bcd? 10000: 65536;

	tc->period  = ((u64)count * 1000000ULL) / PIT_FREQUENCY;
	tc->freq    = (PIT_FREQUENCY * 1000) / count;
	tc->enabled = true;

// 	watch("%d", ch);
// 	watch("%d", tc->period);
// 	watch("%d", tc->freq);

}



uint pit_channel_status(PIT *pit, uint ch)
{

	if (ch >= PIT_NUM_CHANNELS)
		return 0;

	return pit->channel[ch].cfg & 0x3f;

}



void pit_io_rd(PIT *pit, u16 port, uint mode, uint *value)
{

	switch (port) {

		case 0x40: *value = pit_channel_read(pit, 0); break;
		case 0x41: *value = pit_channel_read(pit, 1); break;
		case 0x42: *value = pit_channel_read(pit, 2); break;

	}

	//IODEBUG("PIT");

}



void pit_io_wr(PIT *pit, u16 port, uint mode, uint *value)
{

	switch (port) {

		case 0x40: pit_channel_write(pit, 0, *value); break;
		case 0x41: pit_channel_write(pit, 1, *value); break;
		case 0x42: pit_channel_write(pit, 2, *value); break;
		case 0x43: pit_command(      pit,    *value); break;

	}

	//IODEBUG("PIT");

}

