

#ifndef DEVICE_RAM_H
#define DEVICE_RAM_H


typedef struct memory RAM;


void ram_alloc(RAM *ram, u32 length);
void ram_free( RAM *ram);

void ram_load(RAM *ram, u32 addr, u32 length, const char *path);
void ram_save(RAM *ram, u32 addr, u32 length, const char *path);
u8   ram_peek(RAM *ram, u32 addr);
void ram_poke(RAM *ram, u32 addr, u8 value);


#endif

