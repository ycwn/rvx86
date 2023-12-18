

#ifndef CORE_MEMORY_H
#define CORE_MEMORY_H


struct memory {

	u8  *base;
	u8  *limit;
	u32  length;
	u32  mask;

};



static inline void memory_init(struct memory *m) {
	static u8 guard;
	m->base   = &guard; m->limit = &guard;
	m->length = 1;      m->mask  = 0;
}

static inline struct memory memory_make(void *base, u32 length) {
	struct memory m = { .base=(u8*)base, .limit=(u8*)base + length - 1, .length=length, .mask=length-1 };
	return m;
}

static inline void memory_a20gate(struct memory *m, bool gate) {
	if (gate) m->mask |=   1 << 20;
	else      m->mask &= ~(1 << 20);
}

static inline struct memory memory_mkview(struct memory *m, u32 length, u32 offset) {
	return memory_make(m->base + offset, length);
}


#endif

