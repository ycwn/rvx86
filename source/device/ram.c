

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "core/types.h"
#include "core/debug.h"
#include "core/bus.h"

#include "device/ram.h"

#include "util/fs.h"



void ram_alloc(struct ram *ram, size_t size)
{

	ram->memory = malloc(size);
	ram->size   = size;

}



void ram_free(struct ram *ram)
{

	free(ram->memory);

	ram->memory = NULL;
	ram->size   = 0;

}



int ram_rd(struct ram *ram, u32 addr, uint mode, uint *value)
{

	uint v = 0;

	if (addr < ram->size)
		v = ram->memory[addr];

	if (BUS_IS_RD16(mode)) {

		addr++;

		if (addr < ram->size)
			v |= ram->memory[addr] << 8;

	}

	*value = v;
	return 0;

}



int ram_wr(struct ram *ram, u32 addr, uint mode, uint *value)
{

	if (addr < ram->size)
		ram->memory[addr] = *value >> 0;

	if (BUS_IS_WR16(mode)) {

		addr++;

		if (addr < ram->size)
			ram->memory[addr] = *value >> 8;

	}

	return 0;

}



void ram_load(struct ram *ram, u32 addr, size_t len, const char *path)
{

	fs_load(path, "RAM image", ram->memory + addr, (len < ram->size - addr)? len: ram->size - addr, 1);

}



void ram_save(struct ram *ram, u32 addr, size_t len, const char *path)
{

	fs_save(path, "RAM image", ram->memory + addr, (len < ram->size - addr)? len: ram->size - addr, 1);

}



u8 ram_peek(struct ram *ram, u32 addr)
{

	return (addr < ram->size)? ram->memory[addr]: 0x00;

}



void ram_poke(struct ram *ram, u32 addr, const u8 value)
{

	if (addr < ram->size)
		ram->memory[addr] = value;

}

