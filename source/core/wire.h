

#ifndef CORE_WIRE_H
#define CORE_WIRE_H


typedef int (wire_fn)(void *data, uint id, uint value);


struct wire {

	wire_fn *fn;

	void *data;
	uint  id;

};


#define MKWIRE(f, d, i) { .fn=(wire_fn*)(f), .data=(d), .id=(i) }



static int wire_nop(void *data, uint id, uint arg) {
	return -1;
}

static inline void wire_init(struct wire *w) {
	w->fn   = &wire_nop;
	w->data = NULL;
	w->id   = 0;
}

static inline int wire_act(struct wire *w) {
	return (*w->fn)(w->data, w->id, 0);
}

static inline int wire_actv(struct wire *w, uint v) {
	return (*w->fn)(w->data, w->id, v);
}


#endif

