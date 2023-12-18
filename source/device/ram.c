

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "core/types.h"
#include "core/debug.h"
#include "core/io.h"
#include "core/memory.h"

#include "device/ram.h"

#include "util/fs.h"



void ram_alloc(RAM *ram, u32 length)
{

	ram->base   = malloc(length);
	ram->length = length;
	ram->limit  = ram->base + length;
	ram->mask   = length - 1;

}



void ram_free(RAM *ram)
{

	free(ram->base);
	memory_init(ram);

}



void ram_load(RAM *ram, u32 addr, u32 len, const char *path)
{

	fs_load(path, "RAM image", ram->base + addr, (len < ram->length - addr)? len: ram->length - addr, 1);

}



void ram_save(RAM *ram, u32 addr, u32 len, const char *path)
{

	fs_save(path, "RAM image", ram->base + addr, (len < ram->length - addr)? len: ram->length - addr, 1);

}



u8 ram_peek(RAM *ram, u32 addr)
{

	return (addr < ram->length)? ram->base[addr]: 0x00;

}



void ram_poke(RAM *ram, u32 addr, const u8 value)
{

	if (addr < ram->length)
		ram->base[addr] = value;

}

