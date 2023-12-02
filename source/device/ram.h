

#ifndef DEVICE_RAM_H
#define DEVICE_RAM_H


struct ram {

	u8     *memory;
	size_t  size;

};


void ram_alloc(struct ram *ram, size_t size);
void ram_free( struct ram *ram);

int ram_rd(struct ram *ram, u32 addr, uint mode, uint *value);
int ram_wr(struct ram *ram, u32 addr, uint mode, uint *value);

void ram_load(struct ram *ram, u32 addr, size_t len, const char *path);
void ram_save(struct ram *ram, u32 addr, size_t len, const char *path);
u8   ram_peek(struct ram *ram, u32 addr);
void ram_poke(struct ram *ram, u32 addr, u8 value);


#endif

