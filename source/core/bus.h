

#ifndef CORE_BUS_H
#define CORE_BUS_H


enum {

	BUS_RD8,  BUS_WR8,
	BUS_RD16, BUS_WR16

};


#define BUS_IS_8BIT(x)  (((x) == BUS_RD8)  || ((x) == BUS_WR8))
#define BUS_IS_16BIT(x) (((x) == BUS_RD16) || ((x) == BUS_WR16))
#define BUS_IS_RD(x)    (((x) == BUS_RD8)  || ((x) == BUS_RD16))
#define BUS_IS_WR(x)    (((x) == BUS_WR8)  || ((x) == BUS_WR16))

#define BUS_IS_RD8(x)   ((x) == BUS_RD8)
#define BUS_IS_RD16(x)  ((x) == BUS_RD16)
#define BUS_IS_WR8(x)   ((x) == BUS_WR8)
#define BUS_IS_WR16(x)  ((x) == BUS_WR16)


typedef int (bus_cb)(void *data, u32 addr, uint mode, uint *value);


struct bus {

	void *data;

	bus_cb *rd;
	bus_cb *wr;

};



#define BUS_VCALL(bus, func, ...) \
	((bus)->func((bus)->data, ##__VA_ARGS__))


static inline int bus_nop(void *data, u32 addr, uint mode, uint *value) {
	return 0;
}


static inline void bus_init(struct bus *b) {
	b->data = NULL;
	b->rd   = &bus_nop;
	b->wr   = &bus_nop;
}


static inline uint bus_read8(struct bus *b, u32 addr) {
	uint value = 0;
	BUS_VCALL(b, rd, addr, BUS_RD8, &value);
	return value;
}


static inline uint bus_read16(struct bus *b, u32 addr) {
	uint value = 0;
	BUS_VCALL(b, rd, addr, BUS_RD16, &value);
	return value;
}


static inline void bus_write8(struct bus *b, u32 addr, uint value) {
	BUS_VCALL(b, wr, addr, BUS_WR8, &value);
}


static inline void bus_write16(struct bus *b, u32 addr, uint value) {
	BUS_VCALL(b, wr, addr, BUS_WR16, &value);
}


#endif

