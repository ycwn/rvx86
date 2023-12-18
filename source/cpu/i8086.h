

#ifndef CPU_I8086_H
#define CPU_I8086_H


enum {

	REG_CS = 1, REG_DS = 3, REG_ES = 0, REG_SS = 2,

	REG_ZERO = 4,
	REG_IP   = 7,

	REG_AX = 8,  REG_BX = 11, REG_CX =  9, REG_DX = 10,
	REG_AL = 16, REG_BL = 19, REG_CL = 17, REG_DL = 18,
	REG_AH = 20, REG_BH = 23, REG_CH = 21, REG_DH = 22,

	REG_SI = 14, REG_DI = 15,
	REG_BP = 13, REG_SP = 12,

	REG_FLAGS = 32,

	REG_SHADOW_CS = 33,
	REG_SHADOW_IP = 34

};


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


struct i8086;


typedef void (*i8086_opcode)(struct i8086 *cpu);


typedef struct i8086 {

	// CPU register state
	struct {

		u16 zero;
		u16 scs, sip;
		u16 ip;
		//u16 es, cs, ss, ds, zs;

		i8086_reg ax, bx, cx, dx;
		i8086_reg si, di, bp, sp;

	} regs;


	// CPU status flags
	struct {

		bool c;  // Carry
		bool p;  // Parity
		bool a;  // Aux carry
		bool z;  // Zero
		bool s;  // Sign
		bool v;  // Overflow

		bool d;  // Direction
		bool t;  // Trap
		bool i;  // Interrupt

	} flags;


	// CPU microcode
	struct {

		//i8086_opcode uops[352];
		//uint         mods[352];

		struct {

			bool memory;

			uint  ea_seg;
			u16  *ea_reg0;
			u16  *ea_reg1;
			bool  ea_disp8;
			bool  ea_disp16;

			u8  *regb;
			u16 *regw;
			u16 *regs;

		} demodrm[32];

	} microcode;


	// Interrupt state
	struct {

		uint irq;

		bool irq_act;
		bool nmi_act;
		bool delay;

	} interrupt;


	// Instruction decoder state
	struct {

		bool fetch;
		bool repeat_eq;
		bool repeat_ne;

		bool op_override;
		bool op_memory;
		bool op_segment;

		uint opcode;
		uint modrm;
		uint segment;

		u32 addr;

		u8  *reg0b, *reg1b;
		u16 *reg0w, *reg1w;

	} insn;


	// Memory interface
	struct {

		struct memory mem;

		u16           selector[5];
		struct memory descriptor[5];

	} memory;


	struct io iob;
	struct io iow;

	i8086_opcode undef;

} i8086;



void i8086_init( i8086 *cpu);
void i8086_reset(i8086 *cpu);
int  i8086_intrq(i8086 *cpu, uint nmi, uint irq);
void i8086_tick( i8086 *cpu);

uint i8086_reg_get(i8086 *cpu, uint reg);
void i8086_reg_set(i8086 *cpu, uint reg, uint value);


static inline struct wire i8086_mkirq(i8086 *cpu) {
	return wire_make((wire_fn*)&i8086_intrq, cpu, 0);
}


static inline struct wire i8086_mknmi(i8086 *cpu) {
	return wire_make((wire_fn*)&i8086_intrq, cpu, 1);
}


#endif

