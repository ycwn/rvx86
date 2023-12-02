

#ifndef CPU_I8086_H
#define CPU_I8086_H


enum {
	I8086_VECTOR_DIVERR = 0,
	I8086_VECTOR_SSTEP  = 1,
	I8086_VECTOR_NMI    = 2,
	I8086_VECTOR_BREAK  = 3,
	I8086_VECTOR_VFLOW  = 4
};


typedef union {

	struct {
		u8 l, h;
	};

	u16 w;

} i8086_reg;


struct i8086 {

	// CPU register state
	struct {

		u16 cs, scs;
		u16 ip, sip;
		u16 ds, es, ss;

		i8086_reg ax, bx, cx, dx;
		i8086_reg bp, sp, si, di;

	} regs;


	// CPU status flags
	union {

		struct {

			unsigned c:  1;  // Carry
			unsigned:    1;
			unsigned p:  1;  // Parity
			unsigned:    1;

			unsigned a:  1;  // Aux carry
			unsigned:    1;
			unsigned z:  1;  // Zero
			unsigned s:  1;  // Sign

			unsigned t:  1;  // Trap
			unsigned i:  1;  // Interrupt
			unsigned d:  1;  // Direction
			unsigned v:  1;  // Overflow

		};

		u16 w;

	} flags;


	// Interrupt state
	struct {

		uint irq;

		bool irq_act: 1;
		bool nmi_act: 1;

	} interrupt;


	// Instruction decoder state
	struct {

		bool fetch_opcode:   1;
		bool fetch_modrm:    1;
		bool fetch_simm8a:   1;
		bool fetch_uimm16a:  1;
		bool fetch_simm8b:   1;
		bool fetch_uimm16b:  1;
		bool compute_eaimm0: 1;
		bool compute_easeg:  1;
		bool compute_neaseg: 1; // LEA hack
		bool repeat_eq:      1;
		bool repeat_ne:      1;
		bool complete:       1;

		bool op_wide:     1;
		bool op_reverse:  1;
		bool op_rseg:     1;
		bool op_group:    1;
		bool op_override: 1;
		bool op_memory:   1;

		uint opcode;
		uint modrm;
		uint segment;

		u32 addr;
		u16 imm0, imm1;

		u8  *reg0b, *reg1b;
		u16 *reg0w, *reg1w;

	} insn;


	struct bus io;
	struct bus mem;

};



void i8086_init(struct i8086 *cpu);
void i8086_reset(struct i8086 *cpu);
int  i8086_intrq(struct i8086 *cpu, uint nmi, uint irq);
void i8086_interrupt(struct i8086 *cpu, uint irq);
void i8086_tick(struct i8086 *cpu);
void i8086_dump(struct i8086 *cpu);

extern void i8086_undef(struct i8086 *cpu);


static inline struct wire i8086_irq(struct i8086 *cpu) {
	struct wire w = {
		.fn=(wire_fn*)&i8086_intrq, .data=cpu, .id=0
	};
	return w;
}


static inline struct wire i8086_nmi(struct i8086 *cpu) {
	struct wire w = {
		.fn=(wire_fn*)&i8086_intrq, .data=cpu, .id=1
	};
	return w;
}


#endif

