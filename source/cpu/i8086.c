

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "core/types.h"
#include "core/debug.h"
#include "core/io.h"
#include "core/memory.h"
#include "core/wire.h"


#include "cpu/i8086.h"


typedef u32 ureg;
typedef i32 ireg;


enum {

	OP_MODRM = 1 << 8,  // Requires MODRM
	OP_W8    = 0 << 9,  // Is 8-bit operation
	OP_W16   = 1 << 9,  // Is 16-bit operation
	OP_RSEG  = 1 << 10, // REG in MODRM is segment register
	OP_GROUP = 1 << 11, // Group opcode, REG field of MODRM is operation
	OP_OVRD  = 1 << 12, // Opcode is override, keep flags
	OP_R02   = 1 << 13, // Lowest 3 bits of opcode is register

	OP_RMB  = OP_MODRM | OP_W8,
	OP_RMW  = OP_MODRM | OP_W16,
	OP_R02B = OP_R02   | OP_W8,
	OP_R02W = OP_R02   | OP_W16,
	OP_SEGW = OP_MODRM | OP_W16 | OP_RSEG,

	OP_G0  =  0 | OP_RMB | OP_GROUP,
	OP_G1  =  1 | OP_RMW | OP_GROUP,
	OP_G2  =  2 | OP_RMB | OP_GROUP,
	OP_G3  =  3 | OP_RMW | OP_GROUP,
	OP_G4  =  4 | OP_RMB | OP_GROUP,
	OP_G5  =  5 | OP_RMW | OP_GROUP,
	OP_G6  =  6 | OP_RMB | OP_GROUP,
	OP_G7  =  7 | OP_RMW | OP_GROUP,
	OP_G8  =  8 | OP_RMB | OP_GROUP,
	OP_G9  =  9 | OP_RMW | OP_GROUP,
	OP_G10 = 10 | OP_RMB | OP_GROUP,
	OP_G11 = 11 | OP_RMW | OP_GROUP,


	OP_MODRM_REG_AX_AL = 0 << 0,
	OP_MODRM_REG_BX_BL = 1 << 0,
	OP_MODRM_REG_CX_CL = 2 << 0,
	OP_MODRM_REG_DX_DL = 3 << 0,
	OP_MODRM_REG_SI_DH = 4 << 0,
	OP_MODRM_REG_DI_BH = 5 << 0,
	OP_MODRM_REG_SP_AH = 6 << 0,
	OP_MODRM_REG_BP_CH = 7 << 0,

	OP_MODRM_REG_CS = 0 << 3,
	OP_MODRM_REG_DS = 1 << 3,
	OP_MODRM_REG_ES = 2 << 3,
	OP_MODRM_REG_SS = 3 << 3,

	OP_MODRM_REG_GENERIC = 7 << 0,
	OP_MODRM_REG_SEGMENT = 3 << 3,

	OP_MODRM_REGISTER = 0 << 5,
	OP_MODRM_MEMORY   = 1 << 5,

	OP_MODRM_SEG_DS = 0 << 6,
	OP_MODRM_SEG_SS = 1 << 6,

	OP_MODRM_EA_R0      = 7 << 7,
	OP_MODRM_EA_R0_NONE = 0 << 7,
	OP_MODRM_EA_R0_BX   = 1 << 7,
	OP_MODRM_EA_R0_BP   = 2 << 7,
	OP_MODRM_EA_R0_SI   = 3 << 7,
	OP_MODRM_EA_R0_DI   = 4 << 7,

	OP_MODRM_EA_R1      = 7 << 10,
	OP_MODRM_EA_R1_NONE = 0 << 10,
	OP_MODRM_EA_R1_BX   = 1 << 10,
	OP_MODRM_EA_R1_BP   = 2 << 10,
	OP_MODRM_EA_R1_SI   = 3 << 10,
	OP_MODRM_EA_R1_DI   = 4 << 10,

	OP_MODRM_EA_DISP8  = 1 << 13,
	OP_MODRM_EA_DISP16 = 1 << 14

};


#define CPU  i8086 *cpu

#define SEGMENT(s)  (cpu->memory.selector[(s)])

#define UNDEF(x)   do { if (!(x)) return op_undef(cpu); } while (0)
#define REGONLY()  UNDEF(!cpu->insn.op_memory)
#define MEMONLY()  UNDEF(cpu->insn.op_memory)

#define ADVSP(x)  do { cpu->regs.sp.w += (x); } while (0)

#define ADVSIB()  do { cpu->regs.si.w += (cpu->flags.d)? -1: +1; } while (0)
#define ADVDIB()  do { cpu->regs.di.w += (cpu->flags.d)? -1: +1; } while (0)
#define ADVSIW()  do { cpu->regs.si.w += (cpu->flags.d)? -2: +2; } while (0)
#define ADVDIW()  do { cpu->regs.di.w += (cpu->flags.d)? -2: +2; } while (0)

#define LOCKR0MB()  u8  *ptr = ((cpu->insn.op_memory)? (u8*) memlock(cpu, cpu->insn.segment, cpu->insn.addr, 1): cpu->insn.reg0b)
#define LOCKR1MB()  u8  *ptr = ((cpu->insn.op_memory)? (u8*) memlock(cpu, cpu->insn.segment, cpu->insn.addr, 1): cpu->insn.reg1b)
#define LOCKR0MW()  u16 *ptr = ((cpu->insn.op_memory)? (u16*)memlock(cpu, cpu->insn.segment, cpu->insn.addr, 2): cpu->insn.reg0w)
#define LOCKR1MW()  u16 *ptr = ((cpu->insn.op_memory)? (u16*)memlock(cpu, cpu->insn.segment, cpu->insn.addr, 2): cpu->insn.reg1w)

#define LDLCKM()   (*ptr)
#define STLCKM(x)  do { *ptr = (x); } while (0)

#define LDMB(seg, ofs)  (*(u8*) memlock(cpu, (seg), (ofs), 1))
#define LDMW(seg, ofs)  (*(u16*)memlock(cpu, (seg), (ofs), 2))

#define STMB(seg, ofs, v)  do { *(u8*) memlock(cpu, (seg), ofs, 1) = (v); } while (0)
#define STMW(seg, ofs, v)  do { *(u16*)memlock(cpu, (seg), ofs, 2) = (v); } while (0)

#define LDIPUB()  (cpu->regs.ip += 1, LDMB(REG_CS, cpu->regs.ip - 1))
#define LDIPUW()  (cpu->regs.ip += 2, LDMW(REG_CS, cpu->regs.ip - 2))
#define LDIPSB()   (i8)LDIPUB()
#define LDIPSW()  (i16)LDIPUW()

#define LDEAMB(x)     (LDMB(cpu->insn.segment, cpu->insn.addr + (x)))
#define LDEAMW(x)     (LDMW(cpu->insn.segment, cpu->insn.addr + (x)))
#define STEAMB(x, v)  do { STMB(cpu->insn.segment, cpu->insn.addr + (x), (v)); } while (0)
#define STEAMW(x, v)  do { STMW(cpu->insn.segment, cpu->insn.addr + (x), (v)); } while (0)

#define LDEAR0MB()  ((cpu->insn.op_memory)? LDEAMB(0): *cpu->insn.reg0b)
#define LDEAR1MB()  ((cpu->insn.op_memory)? LDEAMB(0): *cpu->insn.reg1b)
#define LDEAR0MW()  ((cpu->insn.op_memory)? LDEAMW(0): *cpu->insn.reg0w)
#define LDEAR1MW()  ((cpu->insn.op_memory)? LDEAMW(0): *cpu->insn.reg1w)

#define STEAR0MB(x)  do { if (cpu->insn.op_memory) STEAMB(0, (x)); else *cpu->insn.reg0b = (x); } while (0)
#define STEAR1MB(x)  do { if (cpu->insn.op_memory) STEAMB(0, (x)); else *cpu->insn.reg1b = (x); } while (0)
#define STEAR0MW(x)  do { if (cpu->insn.op_memory) STEAMW(0, (x)); else *cpu->insn.reg0w = (x); } while (0)
#define STEAR1MW(x)  do { if (cpu->insn.op_memory) STEAMW(0, (x)); else *cpu->insn.reg1w = (x); } while (0)

#define LDSPB(n)  LDMB(REG_SS, cpu->regs.sp.w + (n))
#define LDSPW(n)  LDMW(REG_SS, cpu->regs.sp.w + (n))

#define STSPB(n, x)  do { STMB(REG_SS, cpu->regs.sp.w + (n), (x)); } while (0)
#define STSPW(n, x)  do { STMW(REG_SS, cpu->regs.sp.w + (n), (x)); } while (0)


static inline bool bit(   ureg x, uint b)                 { return (x & (1 << b)) != 0; }
static inline ureg mask(  ureg x, uint b)                 { return x & ((1 << b) - 1); }
static inline bool zero(  ureg x, uint n)                 { return mask(x, n) == 0; }
static inline bool sign(  ureg x, uint n)                 { return bit(x, n - 1); }
static inline bool vflow( ureg x, ureg a, ureg b, uint n) { return sign(a, n) == sign(b, n) && sign(b, n) != sign(x, n); }
static inline bool carry( ureg x, uint n)                 { return mask(x, n) != x; }
static inline bool parity(ureg x) {
	x = x ^ (x >> 1);
	x = x ^ (x >> 2);
	x = x ^ (x >> 4);
	return !(x & 1);
}

static inline ureg getf_w(CPU) {
	ureg flags = 0xf002;
	if (cpu->flags.c) flags |= 1 <<  0;
	if (cpu->flags.p) flags |= 1 <<  2;
	if (cpu->flags.a) flags |= 1 <<  4;
	if (cpu->flags.z) flags |= 1 <<  6;
	if (cpu->flags.s) flags |= 1 <<  7;
	if (cpu->flags.t) flags |= 1 <<  8;
	if (cpu->flags.i) flags |= 1 <<  9;
	if (cpu->flags.d) flags |= 1 << 10;
	if (cpu->flags.v) flags |= 1 << 11;
	return flags;
}

static inline void setf_lb(CPU, ureg flags) {
	cpu->flags.c = bit(flags,  0);
	cpu->flags.p = bit(flags,  2);
	cpu->flags.a = bit(flags,  4);
	cpu->flags.z = bit(flags,  6);
	cpu->flags.s = bit(flags,  7);
}

static inline void setf_w(CPU, ureg flags) {
	setf_lb(cpu, flags);
	cpu->flags.t = bit(flags,  8);
	cpu->flags.i = bit(flags,  9);
	cpu->flags.d = bit(flags, 10);
	cpu->flags.v = bit(flags, 11);
}

static inline void setf_pzs(CPU, uint n, ureg x) {
	cpu->flags.p = parity(x);
	cpu->flags.z = zero(x, n);
	cpu->flags.s = sign(x, n);
}

static inline ureg setf_add(CPU, uint n, ureg x, ureg a, ureg b, bool d) {
	cpu->flags.c = carry(x, n);
	cpu->flags.v = vflow(x, a, b, n);
	cpu->flags.a = sign(x ^ a ^ b, 5) != d;
	setf_pzs(cpu, n, x);
	return x;
}

static inline ureg setf_inc(CPU, uint n, ureg x, ureg a, ureg b, bool d) {
	cpu->flags.v = vflow(x, a, b, n);
	cpu->flags.a = sign(x ^ a ^ b, 5) != d;
	setf_pzs(cpu, n, x);
	return x;
}

static inline ureg setf_log(CPU, uint n, ureg x) {
	cpu->flags.c = false;
	cpu->flags.v = false;
	cpu->flags.a = false;
	setf_pzs(cpu, n, x);
	return x;
}

static inline void setf_vxc(CPU, uint n, ureg x) { cpu->flags.v = bit(x, n - 1) ^ cpu->flags.c;  }
static inline void setf_vxt(CPU, uint n, ureg x) { cpu->flags.v = bit(x, n - 1) ^ bit(x, n - 2); }


static inline ureg add8( CPU, ureg a, ureg b) { return setf_add(cpu,  8, a + b,                a, b, false); }
static inline ureg add16(CPU, ureg a, ureg b) { return setf_add(cpu, 16, a + b,                a, b, false); }
static inline ureg adc8( CPU, ureg a, ureg b) { return setf_add(cpu,  8, a + b + cpu->flags.c, a, b, false); }
static inline ureg adc16(CPU, ureg a, ureg b) { return setf_add(cpu, 16, a + b + cpu->flags.c, a, b, false); }

static inline ureg sub8( CPU, ureg a, ureg b) { return setf_add(cpu,  8, a - b,                a, ~b, true); }
static inline ureg sub16(CPU, ureg a, ureg b) { return setf_add(cpu, 16, a - b,                a, ~b, true); }
static inline ureg sbb8( CPU, ureg a, ureg b) { return setf_add(cpu,  8, a - b - cpu->flags.c, a, ~b, true); }
static inline ureg sbb16(CPU, ureg a, ureg b) { return setf_add(cpu, 16, a - b - cpu->flags.c, a, ~b, true); }

static inline ureg inc8( CPU, ureg a) { return setf_inc(cpu,  8, a + 1, a,  1, false); }
static inline ureg inc16(CPU, ureg a) { return setf_inc(cpu, 16, a + 1, a,  1, false); }
static inline ureg dec8( CPU, ureg a) { return setf_inc(cpu,  8, a - 1, a, ~1, true);  }
static inline ureg dec16(CPU, ureg a) { return setf_inc(cpu, 16, a - 1, a, ~1, true);  }

static inline ureg and8(CPU, ureg a, ureg b) { return setf_log(cpu, 8, a & b); }
static inline ureg ior8(CPU, ureg a, ureg b) { return setf_log(cpu, 8, a | b); }
static inline ureg xor8(CPU, ureg a, ureg b) { return setf_log(cpu, 8, a ^ b); }

static inline ureg and16(CPU, ureg a, u16 b) { return setf_log(cpu, 16, a & b); }
static inline ureg ior16(CPU, ureg a, u16 b) { return setf_log(cpu, 16, a | b); }
static inline ureg xor16(CPU, ureg a, u16 b) { return setf_log(cpu, 16, a ^ b); }

static inline u8 shrb(u8 a)         { return a >> 1; }
static inline u8 shlb(u8 a)         { return a << 1; }
static inline u8 sarb(u8 a)         { return (i8)a >> 1; }
static inline u8 salb(u8 a)         { return (i8)a << 1; }
static inline u8 rorb(u8 a)         { return (a >> 1) | (a << 7); }
static inline u8 rolb(u8 a)         { return (a << 1) | (a >> 7); }
static inline u8 rcrb(u8 a, bool c) { return (a >> 1) | (a << 8) | (c << 7); }
static inline u8 rclb(u8 a, bool c) { return (a << 1) | (a >> 8) | c; }

static inline ureg shrw(u16 a)         { return a >> 1; }
static inline ureg shlw(u16 a)         { return a << 1; }
static inline ureg sarw(u16 a)         { return (i16)a >> 1; }
static inline ureg salw(u16 a)         { return (i16)a << 1; }
static inline ureg rorw(u16 a)         { return (a >> 1) | (a << 15); }
static inline ureg rolw(u16 a)         { return (a << 1) | (a >> 15); }
static inline ureg rcrw(u16 a, bool c) { return (a >> 1) | (a << 16) | (c << 15); }
static inline ureg rclw(u16 a, bool c) { return (a << 1) | (a >> 16) | c; }



static ureg rol8(CPU, ureg a, uint b)
{

	while (b >= 8)            { cpu->flags.c = bit(a, 0); b -= 8; }
	for (int n=0; n < b; n++) { cpu->flags.c = bit(a, 7); a = rolb(a); }

	setf_vxc(cpu, 8, a);
	return a;

}



static ureg ror8(CPU, ureg a, uint b)
{

	while (b >= 8)            { cpu->flags.c = bit(a, 7); b -= 8; }
	for (int n=0; n < b; n++) { cpu->flags.c = bit(a, 0); a = rorb(a); }

	setf_vxt(cpu, 8, a);
	return a;

}



static ureg rol16(CPU, ureg a, uint b)
{

	while (b >= 16)           { cpu->flags.c = bit(a,  0); b -= 16; }
	for (int n=0; n < b; n++) { cpu->flags.c = bit(a, 15); a = rolw(a); }

	setf_vxc(cpu, 16, a);
	return a;

}



static ureg ror16(CPU, ureg a, uint b)
{

	while (b >= 16)           { cpu->flags.c = bit(a, 15); b -= 16; }
	for (int n=0; n < b; n++) { cpu->flags.c = bit(a,  0); a = rorw(a); }

	setf_vxt(cpu, 16, a);
	return a;

}



static ureg rcl8(CPU, ureg a, uint b)
{

	bool c = cpu->flags.c;

	while (b >= 9)            { b -= 9; }
	for (int n=0; n < b; n++) { cpu->flags.c = bit(a, 7); a = rclb(a, c); c = cpu->flags.c; }

	setf_vxc(cpu, 8, a);
	return a;

}



static ureg rcr8(CPU, ureg a, uint b)
{

	bool c = cpu->flags.c;

	while (b >= 9)            { b -= 9; }
	for (int n=0; n < b; n++) { cpu->flags.c = bit(a, 0); a = rcrb(a, c); c = cpu->flags.c; }

	setf_vxt(cpu, 8, a);
	return a;

}



static ureg rcl16(CPU, ureg a, uint b)
{

	bool c = cpu->flags.c;

	while (b >= 17)           { b -= 17; }
	for (int n=0; n < b; n++) { cpu->flags.c = bit(a, 15); a = rclw(a, c); c = cpu->flags.c; }

	setf_vxc(cpu, 16, a);
	return a;

}



static ureg rcr16(CPU, ureg a, uint b)
{

	bool c = cpu->flags.c;

	while (b >= 17)           { b -= 17; }
	for (int n=0; n < b; n++) { cpu->flags.c = bit(a, 0); a = rcrw(a, c); c = cpu->flags.c; }

	setf_vxt(cpu, 16, a);
	return a;

}



static ureg shl8(CPU, ureg a, uint b)
{

	if (b < 8) for (int n=0; n < b; n++) { cpu->flags.c = bit(a, 7); a = shlb(a); }
	else       a = 0;

	if (b > 0)
		setf_pzs(cpu, 8, a);

	setf_vxc(cpu, 8, a);
	return a;

}



static ureg shr8(CPU, ureg a, uint b)
{

	if (b < 8) for (int n=0; n < b; n++) { cpu->flags.c = bit(a, 0); a = shrb(a); }
	else       a = 0;

	if (b > 0)
		setf_pzs(cpu, 8, a);

	setf_vxt(cpu, 8, a);
	return a;

}



static ureg shl16(CPU, ureg a, uint b)
{

	if (b < 16) for (int n=0; n < b; n++) { cpu->flags.c = bit(a, 15); a = shlw(a); }
	else        a = 0;

	if (b > 0)
		setf_pzs(cpu, 16, a);

	setf_vxc(cpu, 16, a);
	return a;

}



static ureg shr16(CPU, ureg a, uint b)
{

	if (b < 16) for (int n=0; n < b; n++) { cpu->flags.c = bit(a, 0); a = shrw(a); }
	else       a = 0;

	if (b > 0)
		setf_pzs(cpu, 16, a);

	setf_vxt(cpu, 16, a);
	return a;

}



static ureg sal8(CPU, ureg a, uint b)
{

	if (b < 8) for (int n=0; n < b; n++) { cpu->flags.c = bit(a, 7); a = salb(a); }
	else       a = 0;

	if (b > 0)
		setf_pzs(cpu, 8, a);

	setf_vxc(cpu, 8, a);
	return a;

}



static ureg sar8(CPU, ureg a, uint b)
{

	if (b < 8) for (int n=0; n < b; n++) { cpu->flags.c = bit(a, 0); a = sarb(a); }
	else       a = sign(a, 8)? 0xff: 0;

	if (b > 0)
		setf_pzs(cpu, 8, a);

	setf_vxt(cpu, 8, a);
	return a;

}



static ureg sal16(CPU, ureg a, uint b)
{

	if (b < 16) for (int n=0; n < b; n++) { cpu->flags.c = bit(a, 15); a = salw(a); }
	else        a = 0;

	if (b > 0)
		setf_pzs(cpu, 16, a);

	setf_vxc(cpu, 16, a);
	return a;

}



static ureg sar16(CPU, ureg a, uint b)
{

	if (b < 16) for (int n=0; n < b; n++) { cpu->flags.c = bit(a, 0); a = sarw(a); }
	else       a = sign(a, 16)? 0xffff: 0;

	if (b > 0)
		setf_pzs(cpu, 16, a);

	setf_vxt(cpu, 16, a);
	return a;

}



static inline void *memlock(CPU, uint seg, u16 ofs, uint len) {
	//return cpu->memory.descriptor[seg].base + ofs;
	return cpu->memory.mem.base + ((SEGMENT(seg) * 16 + ofs) & cpu->memory.mem.mask);
}



static void memselect(CPU, uint seg, u16 selector)
{

	auto md = &cpu->memory.descriptor[seg];

	cpu->memory.selector[seg] = selector;

	md->base   = cpu->memory.mem.base + ((selector * 16) & cpu->memory.mem.mask);
	md->limit  = cpu->memory.mem.limit;
	md->length = 65536;
	md->mask   = 65535;

}


static void interrupt(CPU, uint irq, uint rcs, uint rip)
{

	ADVSP(-6);
	STSPW(4, getf_w(cpu));
	STSPW(2, rcs);
	STSPW(0, rip);

	cpu->flags.i = false;
	cpu->flags.t = false;

	cpu->regs.ip = LDMW(REG_ZERO, 4 * (u8)irq + 0);
	memselect(cpu, REG_CS, LDMW(REG_ZERO, 4 * (u8)irq + 2));

}



static void op_undef(CPU) { cpu->undef(cpu); }   // Invalid opcode
static void op_nop(CPU)   { }

static void op_addaib(CPU) { const u8 imm = LDIPUB(); cpu->regs.ax.l = add8(cpu, cpu->regs.ax.l, imm); }
static void op_ioraib(CPU) { const u8 imm = LDIPUB(); cpu->regs.ax.l = ior8(cpu, cpu->regs.ax.l, imm); }
static void op_adcaib(CPU) { const u8 imm = LDIPUB(); cpu->regs.ax.l = adc8(cpu, cpu->regs.ax.l, imm); }
static void op_sbbaib(CPU) { const u8 imm = LDIPUB(); cpu->regs.ax.l = sbb8(cpu, cpu->regs.ax.l, imm); }
static void op_andaib(CPU) { const u8 imm = LDIPUB(); cpu->regs.ax.l = and8(cpu, cpu->regs.ax.l, imm); }
static void op_subaib(CPU) { const u8 imm = LDIPUB(); cpu->regs.ax.l = sub8(cpu, cpu->regs.ax.l, imm); }
static void op_xoraib(CPU) { const u8 imm = LDIPUB(); cpu->regs.ax.l = xor8(cpu, cpu->regs.ax.l, imm); }
static void op_cmpaib(CPU) { const u8 imm = LDIPUB();                  sub8(cpu, cpu->regs.ax.l, imm); }

static void op_addaiw(CPU) { const u16 imm = LDIPUW(); cpu->regs.ax.w = add16(cpu, cpu->regs.ax.w, imm); }
static void op_ioraiw(CPU) { const u16 imm = LDIPUW(); cpu->regs.ax.w = ior16(cpu, cpu->regs.ax.w, imm); }
static void op_adcaiw(CPU) { const u16 imm = LDIPUW(); cpu->regs.ax.w = adc16(cpu, cpu->regs.ax.w, imm); }
static void op_sbbaiw(CPU) { const u16 imm = LDIPUW(); cpu->regs.ax.w = sbb16(cpu, cpu->regs.ax.w, imm); }
static void op_andaiw(CPU) { const u16 imm = LDIPUW(); cpu->regs.ax.w = and16(cpu, cpu->regs.ax.w, imm); }
static void op_subaiw(CPU) { const u16 imm = LDIPUW(); cpu->regs.ax.w = sub16(cpu, cpu->regs.ax.w, imm); }
static void op_xoraiw(CPU) { const u16 imm = LDIPUW(); cpu->regs.ax.w = xor16(cpu, cpu->regs.ax.w, imm); }
static void op_cmpaiw(CPU) { const u16 imm = LDIPUW();                  sub16(cpu, cpu->regs.ax.w, imm); }

static void op_addrmbib(CPU) { LOCKR1MB(); const u8 imm = LDIPUB(); STLCKM(add8(cpu, LDLCKM(), imm)); }
static void op_iorrmbib(CPU) { LOCKR1MB(); const u8 imm = LDIPUB(); STLCKM(ior8(cpu, LDLCKM(), imm)); }
static void op_adcrmbib(CPU) { LOCKR1MB(); const u8 imm = LDIPUB(); STLCKM(adc8(cpu, LDLCKM(), imm)); }
static void op_sbbrmbib(CPU) { LOCKR1MB(); const u8 imm = LDIPUB(); STLCKM(sbb8(cpu, LDLCKM(), imm)); }
static void op_andrmbib(CPU) { LOCKR1MB(); const u8 imm = LDIPUB(); STLCKM(and8(cpu, LDLCKM(), imm)); }
static void op_subrmbib(CPU) { LOCKR1MB(); const u8 imm = LDIPUB(); STLCKM(sub8(cpu, LDLCKM(), imm)); }
static void op_xorrmbib(CPU) { LOCKR1MB(); const u8 imm = LDIPUB(); STLCKM(xor8(cpu, LDLCKM(), imm)); }
static void op_cmprmbib(CPU) { LOCKR1MB(); const u8 imm = LDIPUB();        sub8(cpu, LDLCKM(), imm);  }

static void op_addrmwiw(CPU) { LOCKR1MW(); const u16 imm = LDIPUW(); STLCKM(add16(cpu, LDLCKM(), imm)); }
static void op_iorrmwiw(CPU) { LOCKR1MW(); const u16 imm = LDIPUW(); STLCKM(ior16(cpu, LDLCKM(), imm)); }
static void op_adcrmwiw(CPU) { LOCKR1MW(); const u16 imm = LDIPUW(); STLCKM(adc16(cpu, LDLCKM(), imm)); }
static void op_sbbrmwiw(CPU) { LOCKR1MW(); const u16 imm = LDIPUW(); STLCKM(sbb16(cpu, LDLCKM(), imm)); }
static void op_andrmwiw(CPU) { LOCKR1MW(); const u16 imm = LDIPUW(); STLCKM(and16(cpu, LDLCKM(), imm)); }
static void op_subrmwiw(CPU) { LOCKR1MW(); const u16 imm = LDIPUW(); STLCKM(sub16(cpu, LDLCKM(), imm)); }
static void op_xorrmwiw(CPU) { LOCKR1MW(); const u16 imm = LDIPUW(); STLCKM(xor16(cpu, LDLCKM(), imm)); }
static void op_cmprmwiw(CPU) { LOCKR1MW(); const u16 imm = LDIPUW();        sub16(cpu, LDLCKM(), imm);  }

static void op_addrmwib(CPU) { LOCKR1MW(); const u16 imm = LDIPSB(); STLCKM(add16(cpu, LDLCKM(), imm)); }
static void op_iorrmwib(CPU) { LOCKR1MW(); const u16 imm = LDIPSB(); STLCKM(ior16(cpu, LDLCKM(), imm)); }
static void op_adcrmwib(CPU) { LOCKR1MW(); const u16 imm = LDIPSB(); STLCKM(adc16(cpu, LDLCKM(), imm)); }
static void op_sbbrmwib(CPU) { LOCKR1MW(); const u16 imm = LDIPSB(); STLCKM(sbb16(cpu, LDLCKM(), imm)); }
static void op_andrmwib(CPU) { LOCKR1MW(); const u16 imm = LDIPSB(); STLCKM(and16(cpu, LDLCKM(), imm)); }
static void op_subrmwib(CPU) { LOCKR1MW(); const u16 imm = LDIPSB(); STLCKM(sub16(cpu, LDLCKM(), imm)); }
static void op_xorrmwib(CPU) { LOCKR1MW(); const u16 imm = LDIPSB(); STLCKM(xor16(cpu, LDLCKM(), imm)); }
static void op_cmprmwib(CPU) { LOCKR1MW(); const u16 imm = LDIPSB();        sub16(cpu, LDLCKM(), imm);  }

static void op_addrmbf(CPU) { LOCKR1MB(); *cpu->insn.reg0b = add8(cpu, *cpu->insn.reg0b, LDLCKM()); }
static void op_iorrmbf(CPU) { LOCKR1MB(); *cpu->insn.reg0b = ior8(cpu, *cpu->insn.reg0b, LDLCKM()); }
static void op_adcrmbf(CPU) { LOCKR1MB(); *cpu->insn.reg0b = adc8(cpu, *cpu->insn.reg0b, LDLCKM()); }
static void op_sbbrmbf(CPU) { LOCKR1MB(); *cpu->insn.reg0b = sbb8(cpu, *cpu->insn.reg0b, LDLCKM()); }
static void op_andrmbf(CPU) { LOCKR1MB(); *cpu->insn.reg0b = and8(cpu, *cpu->insn.reg0b, LDLCKM()); }
static void op_subrmbf(CPU) { LOCKR1MB(); *cpu->insn.reg0b = sub8(cpu, *cpu->insn.reg0b, LDLCKM()); }
static void op_xorrmbf(CPU) { LOCKR1MB(); *cpu->insn.reg0b = xor8(cpu, *cpu->insn.reg0b, LDLCKM()); }
static void op_cmprmbf(CPU) { LOCKR1MB();                    sub8(cpu, *cpu->insn.reg0b, LDLCKM()); }

static void op_addrmwf(CPU) { LOCKR1MW(); *cpu->insn.reg0w = add16(cpu, *cpu->insn.reg0w, LDLCKM()); }
static void op_iorrmwf(CPU) { LOCKR1MW(); *cpu->insn.reg0w = ior16(cpu, *cpu->insn.reg0w, LDLCKM()); }
static void op_adcrmwf(CPU) { LOCKR1MW(); *cpu->insn.reg0w = adc16(cpu, *cpu->insn.reg0w, LDLCKM()); }
static void op_sbbrmwf(CPU) { LOCKR1MW(); *cpu->insn.reg0w = sbb16(cpu, *cpu->insn.reg0w, LDLCKM()); }
static void op_andrmwf(CPU) { LOCKR1MW(); *cpu->insn.reg0w = and16(cpu, *cpu->insn.reg0w, LDLCKM()); }
static void op_subrmwf(CPU) { LOCKR1MW(); *cpu->insn.reg0w = sub16(cpu, *cpu->insn.reg0w, LDLCKM()); }
static void op_xorrmwf(CPU) { LOCKR1MW(); *cpu->insn.reg0w = xor16(cpu, *cpu->insn.reg0w, LDLCKM()); }
static void op_cmprmwf(CPU) { LOCKR1MW();                    sub16(cpu, *cpu->insn.reg0w, LDLCKM()); }

static void op_addrmbr(CPU) { LOCKR1MB(); STLCKM(add8(cpu, LDLCKM(), *cpu->insn.reg0b)); }
static void op_iorrmbr(CPU) { LOCKR1MB(); STLCKM(ior8(cpu, LDLCKM(), *cpu->insn.reg0b)); }
static void op_adcrmbr(CPU) { LOCKR1MB(); STLCKM(adc8(cpu, LDLCKM(), *cpu->insn.reg0b)); }
static void op_sbbrmbr(CPU) { LOCKR1MB(); STLCKM(sbb8(cpu, LDLCKM(), *cpu->insn.reg0b)); }
static void op_andrmbr(CPU) { LOCKR1MB(); STLCKM(and8(cpu, LDLCKM(), *cpu->insn.reg0b)); }
static void op_subrmbr(CPU) { LOCKR1MB(); STLCKM(sub8(cpu, LDLCKM(), *cpu->insn.reg0b)); }
static void op_xorrmbr(CPU) { LOCKR1MB(); STLCKM(xor8(cpu, LDLCKM(), *cpu->insn.reg0b)); }
static void op_cmprmbr(CPU) { LOCKR1MB();        sub8(cpu, LDLCKM(), *cpu->insn.reg0b);  }

static void op_addrmwr(CPU) { LOCKR1MW(); STLCKM(add16(cpu, LDLCKM(), *cpu->insn.reg0w)); }
static void op_iorrmwr(CPU) { LOCKR1MW(); STLCKM(ior16(cpu, LDLCKM(), *cpu->insn.reg0w)); }
static void op_adcrmwr(CPU) { LOCKR1MW(); STLCKM(adc16(cpu, LDLCKM(), *cpu->insn.reg0w)); }
static void op_sbbrmwr(CPU) { LOCKR1MW(); STLCKM(sbb16(cpu, LDLCKM(), *cpu->insn.reg0w)); }
static void op_andrmwr(CPU) { LOCKR1MW(); STLCKM(and16(cpu, LDLCKM(), *cpu->insn.reg0w)); }
static void op_subrmwr(CPU) { LOCKR1MW(); STLCKM(sub16(cpu, LDLCKM(), *cpu->insn.reg0w)); }
static void op_xorrmwr(CPU) { LOCKR1MW(); STLCKM(xor16(cpu, LDLCKM(), *cpu->insn.reg0w)); }
static void op_cmprmwr(CPU) { LOCKR1MW();        sub16(cpu, LDLCKM(), *cpu->insn.reg0w);  }

static void op_decrmb(CPU) { LOCKR1MB(); STLCKM(dec8(cpu, LDLCKM())); }
static void op_incrmb(CPU) { LOCKR1MB(); STLCKM(inc8(cpu, LDLCKM())); }

static void op_decrmw(CPU) { LOCKR1MW(); STLCKM(dec16(cpu, LDLCKM())); }
static void op_incrmw(CPU) { LOCKR1MW(); STLCKM(inc16(cpu, LDLCKM())); }

static void op_decrw(CPU) { *cpu->insn.reg0w = dec16(cpu, *cpu->insn.reg0w); }
static void op_incrw(CPU) { *cpu->insn.reg0w = inc16(cpu, *cpu->insn.reg0w); }

static void op_negrmb(CPU) { LOCKR1MB(); STLCKM(sub8( cpu, 0, LDLCKM())); }
static void op_negrmw(CPU) { LOCKR1MW(); STLCKM(sub16(cpu, 0, LDLCKM())); }

static void op_notrmb(CPU) { LOCKR1MB(); STLCKM(~LDLCKM()); }
static void op_notrmw(CPU) { LOCKR1MW(); STLCKM(~LDLCKM()); }

static void op_rolb1(CPU) { LOCKR1MB(); STLCKM(rol8(cpu, LDLCKM(), 1)); }
static void op_rorb1(CPU) { LOCKR1MB(); STLCKM(ror8(cpu, LDLCKM(), 1)); }
static void op_rclb1(CPU) { LOCKR1MB(); STLCKM(rcl8(cpu, LDLCKM(), 1)); }
static void op_rcrb1(CPU) { LOCKR1MB(); STLCKM(rcr8(cpu, LDLCKM(), 1)); }
static void op_shlb1(CPU) { LOCKR1MB(); STLCKM(shl8(cpu, LDLCKM(), 1)); }
static void op_shrb1(CPU) { LOCKR1MB(); STLCKM(shr8(cpu, LDLCKM(), 1)); }
static void op_salb1(CPU) { LOCKR1MB(); STLCKM(sal8(cpu, LDLCKM(), 1)); }
static void op_sarb1(CPU) { LOCKR1MB(); STLCKM(sar8(cpu, LDLCKM(), 1)); }

static void op_rolw1(CPU) { LOCKR1MW(); STLCKM(rol16(cpu, LDLCKM(), 1)); }
static void op_rorw1(CPU) { LOCKR1MW(); STLCKM(ror16(cpu, LDLCKM(), 1)); }
static void op_rclw1(CPU) { LOCKR1MW(); STLCKM(rcl16(cpu, LDLCKM(), 1)); }
static void op_rcrw1(CPU) { LOCKR1MW(); STLCKM(rcr16(cpu, LDLCKM(), 1)); }
static void op_shlw1(CPU) { LOCKR1MW(); STLCKM(shl16(cpu, LDLCKM(), 1)); }
static void op_shrw1(CPU) { LOCKR1MW(); STLCKM(shr16(cpu, LDLCKM(), 1)); }
static void op_salw1(CPU) { LOCKR1MW(); STLCKM(sal16(cpu, LDLCKM(), 1)); }
static void op_sarw1(CPU) { LOCKR1MW(); STLCKM(sar16(cpu, LDLCKM(), 1)); }

static void op_rolbr(CPU) { LOCKR1MB(); STLCKM(rol8(cpu, LDLCKM(), cpu->regs.cx.l)); }
static void op_rorbr(CPU) { LOCKR1MB(); STLCKM(ror8(cpu, LDLCKM(), cpu->regs.cx.l)); }
static void op_rclbr(CPU) { LOCKR1MB(); STLCKM(rcl8(cpu, LDLCKM(), cpu->regs.cx.l)); }
static void op_rcrbr(CPU) { LOCKR1MB(); STLCKM(rcr8(cpu, LDLCKM(), cpu->regs.cx.l)); }
static void op_shlbr(CPU) { LOCKR1MB(); STLCKM(shl8(cpu, LDLCKM(), cpu->regs.cx.l)); }
static void op_shrbr(CPU) { LOCKR1MB(); STLCKM(shr8(cpu, LDLCKM(), cpu->regs.cx.l)); }
static void op_salbr(CPU) { LOCKR1MB(); STLCKM(sal8(cpu, LDLCKM(), cpu->regs.cx.l)); }
static void op_sarbr(CPU) { LOCKR1MB(); STLCKM(sar8(cpu, LDLCKM(), cpu->regs.cx.l)); }

static void op_rolwr(CPU) { LOCKR1MW(); STLCKM(rol16(cpu, LDLCKM(), cpu->regs.cx.l)); }
static void op_rorwr(CPU) { LOCKR1MW(); STLCKM(ror16(cpu, LDLCKM(), cpu->regs.cx.l)); }
static void op_rclwr(CPU) { LOCKR1MW(); STLCKM(rcl16(cpu, LDLCKM(), cpu->regs.cx.l)); }
static void op_rcrwr(CPU) { LOCKR1MW(); STLCKM(rcr16(cpu, LDLCKM(), cpu->regs.cx.l)); }
static void op_shlwr(CPU) { LOCKR1MW(); STLCKM(shl16(cpu, LDLCKM(), cpu->regs.cx.l)); }
static void op_shrwr(CPU) { LOCKR1MW(); STLCKM(shr16(cpu, LDLCKM(), cpu->regs.cx.l)); }
static void op_salwr(CPU) { LOCKR1MW(); STLCKM(sal16(cpu, LDLCKM(), cpu->regs.cx.l)); }
static void op_sarwr(CPU) { LOCKR1MW(); STLCKM(sar16(cpu, LDLCKM(), cpu->regs.cx.l)); }

static void op_movambf(CPU) { cpu->insn.addr += LDIPUW(); cpu->regs.ax.l = LDEAMB(0); }
static void op_movamwf(CPU) { cpu->insn.addr += LDIPUW(); cpu->regs.ax.w = LDEAMW(0); }

static void op_movambr(CPU) { cpu->insn.addr += LDIPUW(); STMB(cpu->insn.segment, cpu->insn.addr, cpu->regs.ax.l); }
static void op_movamwr(CPU) { cpu->insn.addr += LDIPUW(); STMW(cpu->insn.segment, cpu->insn.addr, cpu->regs.ax.w); }

static void op_movrib(CPU)  { *cpu->insn.reg0b = LDIPUB(); }
static void op_movriw(CPU)  { *cpu->insn.reg0w = LDIPUW(); }

static void op_movrmib(CPU) { const u8  imm = LDIPUB(); STEAR1MB(imm); }
static void op_movrmiw(CPU) { const u16 imm = LDIPUW(); STEAR1MW(imm); }

static void op_movrrmbf(CPU) { *cpu->insn.reg0b = LDEAR1MB(); }
static void op_movrrmwf(CPU) { *cpu->insn.reg0w = LDEAR1MW(); }

static void op_movsrmwf(CPU) { memselect(cpu, (uint)cpu->insn.reg0w, LDEAR1MW()); cpu->interrupt.delay = true; }

static void op_movrrmbr(CPU) { STEAR1MB(*cpu->insn.reg0b); }
static void op_movrrmwr(CPU) { STEAR1MW(*cpu->insn.reg0w); }

static void op_movsrmwr(CPU) { STEAR1MW(SEGMENT((uint)cpu->insn.reg0w)); }

static void op_cbw(CPU) { cpu->regs.ax.w = (i8)cpu->regs.ax.l; }
static void op_cwd(CPU) { cpu->regs.dx.w = sign(cpu->regs.ax.w, 16)? 0xffff: 0x0000; }

static void op_lahf(CPU) { cpu->regs.ax.h = getf_w(cpu); }
static void op_sahf(CPU) { setf_lb(cpu, cpu->regs.ax.h); }

static void op_xcharw(CPU) { ureg tmp = cpu->regs.ax.w; cpu->regs.ax.w = *cpu->insn.reg0w; *cpu->insn.reg0w = tmp; }

static void op_xchrmb(CPU) { LOCKR1MB(); ureg tmp = LDLCKM(); STLCKM(*cpu->insn.reg0b); *cpu->insn.reg0b = tmp; }
static void op_xchrmw(CPU) { LOCKR1MW(); ureg tmp = LDLCKM(); STLCKM(*cpu->insn.reg0w); *cpu->insn.reg0w = tmp; }

static void op_tstaib(CPU) { const u8  imm = LDIPUB(); and8( cpu, cpu->regs.ax.l, imm); }
static void op_tstaiw(CPU) { const u16 imm = LDIPUW(); and16(cpu, cpu->regs.ax.w, imm); }

static void op_tstrib(CPU) { const u8  imm = LDIPUB(); ureg tmp = LDEAR1MB(); and8( cpu, tmp, imm); }
static void op_tstriw(CPU) { const u16 imm = LDIPUW(); ureg tmp = LDEAR1MW(); and16(cpu, tmp, imm); }

static void op_tstrmb(CPU) { ureg tmp = LDEAR1MB(); and8( cpu, tmp, *cpu->insn.reg0b); }
static void op_tstrmw(CPU) { ureg tmp = LDEAR1MW(); and16(cpu, tmp, *cpu->insn.reg0w); }

static void op_ldsr(CPU) { MEMONLY(); *cpu->insn.reg0w = LDEAMW(0); memselect(cpu, REG_DS, LDEAMW(2)); }
static void op_lesr(CPU) { MEMONLY(); *cpu->insn.reg0w = LDEAMW(0); memselect(cpu, REG_ES, LDEAMW(2)); }
static void op_lear(CPU) { MEMONLY(); *cpu->insn.reg0w = cpu->insn.addr; }

static void op_xlatab(CPU) { cpu->regs.ax.l = LDMB(cpu->insn.segment, cpu->regs.bx.w + cpu->regs.ax.l); }

static void op_popcs( CPU) { ADVSP(+2); memselect(cpu, REG_CS, LDSPW(-2)); }
static void op_popds( CPU) { ADVSP(+2); memselect(cpu, REG_DS, LDSPW(-2)); }
static void op_popes( CPU) { ADVSP(+2); memselect(cpu, REG_ES, LDSPW(-2)); }
static void op_popss( CPU) { ADVSP(+2); memselect(cpu, REG_SS, LDSPW(-2)); cpu->interrupt.delay = true; }
static void op_poprw( CPU) { ADVSP(+2); *cpu->insn.reg0w = LDSPW(-2); }
static void op_popfw( CPU) { ADVSP(+2); setf_w(cpu, LDSPW(-2)); }

static void op_pushcs(CPU) { ADVSP(-2); STSPW(0, SEGMENT(REG_CS)); }
static void op_pushds(CPU) { ADVSP(-2); STSPW(0, SEGMENT(REG_DS)); }
static void op_pushes(CPU) { ADVSP(-2); STSPW(0, SEGMENT(REG_ES)); }
static void op_pushss(CPU) { ADVSP(-2); STSPW(0, SEGMENT(REG_SS)); }
static void op_pushsp(CPU) { ADVSP(-2); STSPW(0, cpu->regs.sp.w); }
static void op_pushrw(CPU) { ADVSP(-2); STSPW(0, *cpu->insn.reg0w); }
static void op_pushfw(CPU) { ADVSP(-2); STSPW(0, getf_w(cpu)); }

static void op_poprmw(CPU)  { ADVSP(+2); ureg tmp = LDSPW(-2);  STEAR1MW(tmp); }
static void op_pushrmw(CPU) { ADVSP(-2); ureg tmp = LDEAR1MW(); STSPW(0, tmp); }

static void op_callf(CPU) { const u16 tip = LDIPUW(), tcs = LDIPUW(); ADVSP(-4); STSPW(2, SEGMENT(REG_CS)); STSPW(0, cpu->regs.ip); cpu->regs.ip  = tip; SEGMENT(REG_CS) = tcs; }
static void op_calln(CPU) { const i16 imm = LDIPSW();                 ADVSP(-2); STSPW(0, cpu->regs.ip);                         cpu->regs.ip += imm; }
static void op_callfrm(CPU) { MEMONLY(); ureg tip = LDEAMW(0); ureg tcs = LDEAMW(2); ADVSP(-4); STSPW(2, SEGMENT(REG_CS)); STSPW(0, cpu->regs.ip); cpu->regs.ip = tip; SEGMENT(REG_CS) = tcs; }
static void op_callnrm(CPU) { ureg tmp = LDEAR1MW(); ADVSP(-2); STSPW(0, cpu->regs.ip); cpu->regs.ip = tmp; }

static void op_int3(CPU)  {                          interrupt(cpu, I8086_VECTOR_BREAK, SEGMENT(REG_CS), cpu->regs.ip); }
static void op_into(CPU)  { if (cpu->flags.v)        interrupt(cpu, I8086_VECTOR_VFLOW, SEGMENT(REG_CS), cpu->regs.ip); }
static void op_intib(CPU) { const u8 imm = LDIPUB(); interrupt(cpu, imm, SEGMENT(REG_CS), cpu->regs.ip); }
static void op_iret(CPU)  { ADVSP(+4); cpu->regs.ip = LDSPW(-4); memselect(cpu, REG_CS, LDSPW(-2)); op_popfw(cpu); cpu->interrupt.delay = true; }

static void op_jcbe(CPU) { const i8 imm = LDIPSB(); if (cpu->flags.c                 || cpu->flags.z) cpu->regs.ip += imm; }
static void op_jcle(CPU) { const i8 imm = LDIPSB(); if (cpu->flags.s != cpu->flags.v || cpu->flags.z) cpu->regs.ip += imm; }
static void op_jcl(CPU)  { const i8 imm = LDIPSB(); if (cpu->flags.s != cpu->flags.v)                 cpu->regs.ip += imm; }

static void op_jcc(CPU) { const i8 imm = LDIPSB(); if (cpu->flags.c) cpu->regs.ip += imm; }
static void op_jco(CPU) { const i8 imm = LDIPSB(); if (cpu->flags.v) cpu->regs.ip += imm; }
static void op_jcp(CPU) { const i8 imm = LDIPSB(); if (cpu->flags.p) cpu->regs.ip += imm; }
static void op_jcs(CPU) { const i8 imm = LDIPSB(); if (cpu->flags.s) cpu->regs.ip += imm; }
static void op_jcz(CPU) { const i8 imm = LDIPSB(); if (cpu->flags.z) cpu->regs.ip += imm; }

static void op_jcnbe(CPU) { const i8 imm = LDIPSB(); if (!cpu->flags.c                && !cpu->flags.z) cpu->regs.ip += imm; }
static void op_jcnle(CPU) { const i8 imm = LDIPSB(); if (cpu->flags.s == cpu->flags.v && !cpu->flags.z) cpu->regs.ip += imm; }
static void op_jcnl(CPU)  { const i8 imm = LDIPSB(); if (cpu->flags.s == cpu->flags.v)                  cpu->regs.ip += imm; }

static void op_jcnc(CPU) { const i8 imm = LDIPSB(); if (!cpu->flags.c) cpu->regs.ip += imm; }
static void op_jcno(CPU) { const i8 imm = LDIPSB(); if (!cpu->flags.v) cpu->regs.ip += imm; }
static void op_jcnp(CPU) { const i8 imm = LDIPSB(); if (!cpu->flags.p) cpu->regs.ip += imm; }
static void op_jcns(CPU) { const i8 imm = LDIPSB(); if (!cpu->flags.s) cpu->regs.ip += imm; }
static void op_jcnz(CPU) { const i8 imm = LDIPSB(); if (!cpu->flags.z) cpu->regs.ip += imm; }

static void op_jcxzr(CPU) { const i8 imm = LDIPSB(); if (!cpu->regs.cx.w)  cpu->regs.ip += imm; }

static void op_jmpf(CPU)  { const u16 tip = LDIPUW(); memselect(cpu, REG_CS, LDIPUW()); cpu->regs.ip = tip; }
static void op_jmpnw(CPU) { const i16 imm = LDIPSW(); cpu->regs.ip += imm; }
static void op_jmpnb(CPU) { const i8  imm = LDIPSB(); cpu->regs.ip += imm; }
static void op_jmpfrm(CPU) { MEMONLY(); cpu->regs.ip = LDEAMW(0); memselect(cpu, REG_CS, LDEAMW(2)); }
static void op_jmpnrm(CPU) { cpu->regs.ip = LDEAR1MW(); }

static void op_loopnzr(CPU) { const i8 imm = LDIPSB(); if (--cpu->regs.cx.w && !cpu->flags.z) cpu->regs.ip += imm; }
static void op_loopzr(CPU)  { const i8 imm = LDIPSB(); if (--cpu->regs.cx.w && cpu->flags.z)  cpu->regs.ip += imm; }
static void op_loopr(CPU)   { const i8 imm = LDIPSB(); if (--cpu->regs.cx.w)                  cpu->regs.ip += imm; }

static void op_retf0(CPU) { ADVSP(+4); cpu->regs.ip = LDSPW(-4); memselect(cpu, REG_CS, LDSPW(-2)); }
static void op_retn0(CPU) {                           ADVSP(+2); cpu->regs.ip = LDSPW(-2); }

static void op_retfw(CPU) { const u16 imm = LDIPUW(); op_retf0(cpu); ADVSP(imm); }
static void op_retnw(CPU) { const u16 imm = LDIPUW(); op_retn0(cpu); ADVSP(imm); }

static void op_inib(CPU) { const u8 imm = LDIPUB(); cpu->regs.ax.l = io_readb(&cpu->iob, imm); }
static void op_iniw(CPU) { const u8 imm = LDIPUB(); cpu->regs.ax.w = io_readw(&cpu->iow, imm); }
static void op_inrb(CPU) { cpu->regs.ax.l = io_readb(&cpu->iob, cpu->regs.dx.w); }
static void op_inrw(CPU) { cpu->regs.ax.w = io_readw(&cpu->iow, cpu->regs.dx.w); }

static void op_outib(CPU) { const u8 imm = LDIPUB(); io_writeb(&cpu->iob, imm, cpu->regs.ax.l); }
static void op_outiw(CPU) { const u8 imm = LDIPUB(); io_writew(&cpu->iow, imm, cpu->regs.ax.w); }
static void op_outrb(CPU) { io_writeb(&cpu->iob, cpu->regs.dx.w, cpu->regs.ax.l); }
static void op_outrw(CPU) { io_writew(&cpu->iow, cpu->regs.dx.w, cpu->regs.ax.w); }

static void op_seges(CPU) { cpu->insn.segment = REG_ES; }
static void op_segcs(CPU) { cpu->insn.segment = REG_CS; }
static void op_segss(CPU) { cpu->insn.segment = REG_SS; }
static void op_segds(CPU) { cpu->insn.segment = REG_DS; }
static void op_repne(CPU) { cpu->insn.repeat_eq = false; cpu->insn.repeat_ne = true;  }
static void op_repeq(CPU) { cpu->insn.repeat_eq = true;  cpu->insn.repeat_ne = false; }

static void op_lock(CPU) { /* FIXME: Not implemented */ }
static void op_wait(CPU) { /* FIXME: Not implemented */ }

static void op_clc(CPU) { cpu->flags.c = false; }
static void op_stc(CPU) { cpu->flags.c = true;  }
static void op_cli(CPU) { cpu->flags.i = false; }
static void op_cmc(CPU) { cpu->flags.c = !cpu->flags.c; }
static void op_sti(CPU) { cpu->flags.i = true;  }
static void op_cld(CPU) { cpu->flags.d = false; }
static void op_std(CPU) { cpu->flags.d = true;  }

static void op_divrmb(CPU);
static void op_divrmw(CPU);
static void op_mulrmb(CPU);
static void op_mulrmw(CPU);

static void op_idivrmb(CPU);
static void op_idivrmw(CPU);
static void op_imulrmb(CPU);
static void op_imulrmw(CPU);

static void op_aaa(CPU);      // BCD
static void op_aad(CPU);
static void op_aas(CPU);
static void op_aam(CPU);
static void op_daa(CPU);
static void op_das(CPU);

static void op_cmpsb(CPU);   // String
static void op_cmpsw(CPU);
static void op_lodsb(CPU);
static void op_lodsw(CPU);
static void op_movsb(CPU);
static void op_movsw(CPU);
static void op_scasb(CPU);
static void op_scasw(CPU);
static void op_stosb(CPU);
static void op_stosw(CPU);


static const i8086_opcode opcodes[352] = {

// Main opcodes
// 0x00
	&op_addrmbr,  &op_addrmwr,  &op_addrmbf,  &op_addrmwf,  &op_addaib,   &op_addaiw,  &op_pushes,   &op_popes,
	&op_iorrmbr,  &op_iorrmwr,  &op_iorrmbf,  &op_iorrmwf,  &op_ioraib,   &op_ioraiw,  &op_pushcs,   &op_popcs,
	&op_adcrmbr,  &op_adcrmwr,  &op_adcrmbf,  &op_adcrmwf,  &op_adcaib,   &op_adcaiw,  &op_pushss,   &op_popss,
	&op_sbbrmbr,  &op_sbbrmwr,  &op_sbbrmbf,  &op_sbbrmwf,  &op_sbbaib,   &op_sbbaiw,  &op_pushds,   &op_popds,
// 0x20
	&op_andrmbr,  &op_andrmwr,  &op_andrmbf,  &op_andrmwf,  &op_andaib,   &op_andaiw,  &op_seges,    &op_daa,
	&op_subrmbr,  &op_subrmwr,  &op_subrmbf,  &op_subrmwf,  &op_subaib,   &op_subaiw,  &op_segcs,    &op_das,
	&op_xorrmbr,  &op_xorrmwr,  &op_xorrmbf,  &op_xorrmwf,  &op_xoraib,   &op_xoraiw,  &op_segss,    &op_aaa,
	&op_cmprmbr,  &op_cmprmwr,  &op_cmprmbf,  &op_cmprmwf,  &op_cmpaib,   &op_cmpaiw,  &op_segds,    &op_aas,
// 0x40
	&op_incrw,    &op_incrw,    &op_incrw,    &op_incrw,    &op_incrw,    &op_incrw,   &op_incrw,    &op_incrw,
	&op_decrw,    &op_decrw,    &op_decrw,    &op_decrw,    &op_decrw,    &op_decrw,   &op_decrw,    &op_decrw,
	&op_pushrw,   &op_pushrw,   &op_pushrw,   &op_pushrw,   &op_pushsp,   &op_pushrw,  &op_pushrw,   &op_pushrw,
	&op_poprw,    &op_poprw,    &op_poprw,    &op_poprw,    &op_poprw,    &op_poprw,   &op_poprw,    &op_poprw,
// 0x60
	&op_jco,      &op_jcno,     &op_jcc,      &op_jcnc,     &op_jcz,      &op_jcnz,    &op_jcbe,     &op_jcnbe,
	&op_jcs,      &op_jcns,     &op_jcp,      &op_jcnp,     &op_jcl,      &op_jcnl,    &op_jcle,     &op_jcnle,
	&op_jco,      &op_jcno,     &op_jcc,      &op_jcnc,     &op_jcz,      &op_jcnz,    &op_jcbe,     &op_jcnbe,
	&op_jcs,      &op_jcns,     &op_jcp,      &op_jcnp,     &op_jcl,      &op_jcnl,    &op_jcle,     &op_jcnle,
// 0x80
	&op_nop,      &op_nop,      &op_nop,      &op_nop,      &op_tstrmb,   &op_tstrmw,  &op_xchrmb,   &op_xchrmw,
	&op_movrrmbr, &op_movrrmwr, &op_movrrmbf, &op_movrrmwf, &op_movsrmwr, &op_lear,    &op_movsrmwf, &op_poprmw,
	&op_xcharw,   &op_xcharw,   &op_xcharw,   &op_xcharw,   &op_xcharw,   &op_xcharw,  &op_xcharw,   &op_xcharw,
	&op_cbw,      &op_cwd,      &op_callf,    &op_wait,     &op_pushfw,   &op_popfw,   &op_sahf,     &op_lahf,
// 0xa0
	&op_movambf,  &op_movamwf,  &op_movambr,  &op_movamwr,  &op_movsb,    &op_movsw,   &op_cmpsb,    &op_cmpsw,
	&op_tstaib,   &op_tstaiw,   &op_stosb,    &op_stosw,    &op_lodsb,    &op_lodsw,   &op_scasb,    &op_scasw,
	&op_movrib,   &op_movrib,   &op_movrib,   &op_movrib,   &op_movrib,   &op_movrib,  &op_movrib,   &op_movrib,
	&op_movriw,   &op_movriw,   &op_movriw,   &op_movriw,   &op_movriw,   &op_movriw,  &op_movriw,   &op_movriw,
// 0xc0
	&op_undef,    &op_undef,    &op_retnw,    &op_retn0,    &op_lesr,     &op_ldsr,    &op_movrmib,  &op_movrmiw,
	&op_undef,    &op_undef,    &op_retfw,    &op_retf0,    &op_int3,     &op_intib,   &op_into,     &op_iret,
	&op_nop,      &op_nop,      &op_nop,      &op_nop,      &op_aam,      &op_aad,     &op_undef,    &op_xlatab,
	&op_nop,      &op_nop,      &op_nop,      &op_nop,      &op_nop,      &op_nop,     &op_nop,      &op_nop,
// 0xe0
	&op_loopnzr,  &op_loopzr,   &op_loopr,    &op_jcxzr,    &op_inib,     &op_iniw,    &op_outib,    &op_outiw,
	&op_calln,    &op_jmpnw,    &op_jmpf,     &op_jmpnb,    &op_inrb,     &op_inrw,    &op_outrb,    &op_outrw,
	&op_lock,     &op_undef,    &op_repne,    &op_repeq,    &op_nop,      &op_cmc,     &op_nop,      &op_nop,
	&op_clc,      &op_stc,      &op_cli,      &op_sti,      &op_cld,      &op_std,     &op_nop,      &op_nop,

// Group opcodes
// 0x100
	&op_addrmbib, &op_iorrmbib, &op_adcrmbib, &op_sbbrmbib, &op_andrmbib, &op_subrmbib, &op_xorrmbib, &op_cmprmbib,  // 0:  0x80
	&op_addrmwiw, &op_iorrmwiw, &op_adcrmwiw, &op_sbbrmwiw, &op_andrmwiw, &op_subrmwiw, &op_xorrmwiw, &op_cmprmwiw,  // 1:  0x81
	&op_addrmbib, &op_iorrmbib, &op_adcrmbib, &op_sbbrmbib, &op_andrmbib, &op_subrmbib, &op_xorrmbib, &op_cmprmbib,  // 2:  0x82
	&op_addrmwib, &op_iorrmwib, &op_adcrmwib, &op_sbbrmwib, &op_andrmwib, &op_subrmwib, &op_xorrmwib, &op_cmprmwib,  // 3:  0x83
// 0x120
	&op_rolb1,   &op_rorb1,   &op_rclb1,   &op_rcrb1,   &op_shlb1,   &op_shrb1,   &op_salb1,   &op_sarb1,    // 4:  0xd0
	&op_rolw1,   &op_rorw1,   &op_rclw1,   &op_rcrw1,   &op_shlw1,   &op_shrw1,   &op_salw1,   &op_sarw1,    // 5:  0xd1
	&op_rolbr,   &op_rorbr,   &op_rclbr,   &op_rcrbr,   &op_shlbr,   &op_shrbr,   &op_salbr,   &op_sarbr,    // 6:  0xd2
	&op_rolwr,   &op_rorwr,   &op_rclwr,   &op_rcrwr,   &op_shlwr,   &op_shrwr,   &op_salwr,   &op_sarwr,    // 7:  0xd3
// 0x140
	&op_tstrib,  &op_tstrib,  &op_notrmb,  &op_negrmb,  &op_mulrmb,  &op_imulrmb, &op_divrmb,  &op_idivrmb,  // 8:  0xf6
	&op_tstriw,  &op_tstriw,  &op_notrmw,  &op_negrmw,  &op_mulrmw,  &op_imulrmw, &op_divrmw,  &op_idivrmw,  // 9:  0xf7
	&op_incrmb,  &op_decrmb,  &op_undef,   &op_undef,   &op_undef,   &op_undef,   &op_undef,   &op_undef,    // 10: 0xfe
	&op_incrmw,  &op_decrmw,  &op_callnrm, &op_callfrm, &op_jmpnrm,  &op_jmpfrm,  &op_pushrmw, &op_pushrmw   // 11: 0xff

};


static const uint opflags[352] = {

// Main opcodes
// 0x00
	OP_RMB,   OP_RMW,   OP_RMB,   OP_RMW,   OP_W8,    OP_W16,   0,        0,
	OP_RMB,   OP_RMW,   OP_RMB,   OP_RMW,   OP_W8,    OP_W16,   0,        0,
	OP_RMB,   OP_RMW,   OP_RMB,   OP_RMW,   OP_W8,    OP_W16,   0,        0,
	OP_RMB,   OP_RMW,   OP_RMB,   OP_RMW,   OP_W8,    OP_W16,   0,        0,
// 0x20
	OP_RMB,   OP_RMW,   OP_RMB,   OP_RMW,   OP_W8,    OP_W16,   OP_OVRD,  0,
	OP_RMB,   OP_RMW,   OP_RMB,   OP_RMW,   OP_W8,    OP_W16,   OP_OVRD,  0,
	OP_RMB,   OP_RMW,   OP_RMB,   OP_RMW,   OP_W8,    OP_W16,   OP_OVRD,  0,
	OP_RMB,   OP_RMW,   OP_RMB,   OP_RMW,   OP_W8,    OP_W16,   OP_OVRD,  0,
// 0x40
	OP_R02W,  OP_R02W,  OP_R02W,  OP_R02W,  OP_R02W,  OP_R02W,  OP_R02W,  OP_R02W,
	OP_R02W,  OP_R02W,  OP_R02W,  OP_R02W,  OP_R02W,  OP_R02W,  OP_R02W,  OP_R02W,
	OP_R02W,  OP_R02W,  OP_R02W,  OP_R02W,  OP_R02W,  OP_R02W,  OP_R02W,  OP_R02W,
	OP_R02W,  OP_R02W,  OP_R02W,  OP_R02W,  OP_R02W,  OP_R02W,  OP_R02W,  OP_R02W,
// 0x60
	0,        0,        0,        0,        0,        0,        0,        0,
	0,        0,        0,        0,        0,        0,        0,        0,
	0,        0,        0,        0,        0,        0,        0,        0,
	0,        0,        0,        0,        0,        0,        0,        0,
// 0x80
	OP_G0,    OP_G1,    OP_G2,    OP_G3,    OP_RMB,   OP_RMW,   OP_RMB,   OP_RMW,
	OP_RMB,   OP_RMW,   OP_RMB,   OP_RMW,   OP_SEGW,  OP_RMW,   OP_SEGW,  OP_RMW,
	OP_R02W,  OP_R02W,  OP_R02W,  OP_R02W,  OP_R02W,  OP_R02W,  OP_R02W,  OP_R02W,
	0,        0,        0,        0,        0,        0,        0,        0,
// 0xa0
	OP_W8,    OP_W16,   OP_W8,    OP_W16,   OP_W8,    OP_W16,   OP_W8,    OP_W16,
	OP_W8,    OP_W16,   OP_W8,    OP_W16,   OP_W8,    OP_W16,   0,        0,
	OP_R02B,  OP_R02B,  OP_R02B,  OP_R02B,  OP_R02B,  OP_R02B,  OP_R02B,  OP_R02B,
	OP_R02W,  OP_R02W,  OP_R02W,  OP_R02W,  OP_R02W,  OP_R02W,  OP_R02W,  OP_R02W,
// 0xc0
	0,        0,        0,        0,        OP_RMW,   OP_RMW,   OP_RMB,   OP_RMW,
	0,        0,        0,        0,        0,        0,        0,        0,
	OP_G4,    OP_G5,    OP_G6,    OP_G7,    OP_W8,    OP_W8,    0,        0,
	OP_MODRM, OP_MODRM, OP_MODRM, OP_MODRM, OP_MODRM, OP_MODRM, OP_MODRM, OP_MODRM,
// 0xe0
	0,        0,        0,        0,        OP_W8,    OP_W16,   OP_W8,    OP_W16,
	0,        0,        0,        0,        OP_W8,    OP_W16,   OP_W8,    OP_W16,
	OP_OVRD,  0,        OP_OVRD,  OP_OVRD,  0,        0,        OP_G8,    OP_G9,
	0,        0,        0,        0,        0,        0,        OP_G10,   OP_G11,

// Group opcodes
// 0x100
	0,        0,        0,        0,        0,        0,        0,        0,
	0,        0,        0,        0,        0,        0,        0,        0,
	0,        0,        0,        0,        0,        0,        0,        0,
	0,        0,        0,        0,        0,        0,        0,        0,
// 0x120
	0,        0,        0,        0,        0,        0,        0,        0,
	0,        0,        0,        0,        0,        0,        0,        0,
	0,        0,        0,        0,        0,        0,        0,        0,
	0,        0,        0,        0,        0,        0,        0,        0,
// 0x140
	0,        0,        0,        0,        0,        0,        0,        0,
	0,        0,        0,        0,        0,        0,        0,        0,
	0,        0,        0,        0,        0,        0,        0,        0,
	0,        0,        0,        0,        0,        0,        0,        0

};


static const uint demodrm[32] = {

	OP_MODRM_MEMORY | OP_MODRM_SEG_DS | OP_MODRM_EA_R0_BX | OP_MODRM_EA_R1_SI,  // 00: [BX + SI]
	OP_MODRM_MEMORY | OP_MODRM_SEG_DS | OP_MODRM_EA_R0_BX | OP_MODRM_EA_R1_DI,  // 01: [BX + DI]
	OP_MODRM_MEMORY | OP_MODRM_SEG_SS | OP_MODRM_EA_R0_BP | OP_MODRM_EA_R1_SI,  // 02: [BP + SI]
	OP_MODRM_MEMORY | OP_MODRM_SEG_SS | OP_MODRM_EA_R0_BP | OP_MODRM_EA_R1_DI,  // 03: [BP + DI]
	OP_MODRM_MEMORY | OP_MODRM_SEG_DS | OP_MODRM_EA_R0_SI,                      // 04: [SI]
	OP_MODRM_MEMORY | OP_MODRM_SEG_DS | OP_MODRM_EA_R0_DI,                      // 05: [DI]
	OP_MODRM_MEMORY | OP_MODRM_SEG_DS | OP_MODRM_EA_DISP16,                     // 06: [DISP16]
	OP_MODRM_MEMORY | OP_MODRM_SEG_DS | OP_MODRM_EA_R0_BX,                      // 07: [BX]

	OP_MODRM_MEMORY | OP_MODRM_SEG_DS | OP_MODRM_EA_R0_BX | OP_MODRM_EA_R1_SI | OP_MODRM_EA_DISP8,   // 40: [BX + SI + DISP8]
	OP_MODRM_MEMORY | OP_MODRM_SEG_DS | OP_MODRM_EA_R0_BX | OP_MODRM_EA_R1_DI | OP_MODRM_EA_DISP8,   // 41: [BX + DI + DISP8]
	OP_MODRM_MEMORY | OP_MODRM_SEG_SS | OP_MODRM_EA_R0_BP | OP_MODRM_EA_R1_SI | OP_MODRM_EA_DISP8,   // 42: [BP + SI + DISP8]
	OP_MODRM_MEMORY | OP_MODRM_SEG_SS | OP_MODRM_EA_R0_BP | OP_MODRM_EA_R1_DI | OP_MODRM_EA_DISP8,   // 43: [BP + DI + DISP8]
	OP_MODRM_MEMORY | OP_MODRM_SEG_DS | OP_MODRM_EA_R0_SI                     | OP_MODRM_EA_DISP8,   // 44: [SI + DISP8]
	OP_MODRM_MEMORY | OP_MODRM_SEG_DS | OP_MODRM_EA_R0_DI                     | OP_MODRM_EA_DISP8,   // 45: [DI + DISP8]
	OP_MODRM_MEMORY | OP_MODRM_SEG_SS | OP_MODRM_EA_R0_BP                     | OP_MODRM_EA_DISP8,   // 46: [BP + DISP8]
	OP_MODRM_MEMORY | OP_MODRM_SEG_DS | OP_MODRM_EA_R0_BX                     | OP_MODRM_EA_DISP8,   // 47: [BX + DISP8]

	OP_MODRM_MEMORY | OP_MODRM_SEG_DS | OP_MODRM_EA_R0_BX | OP_MODRM_EA_R1_SI | OP_MODRM_EA_DISP16,  // 80: [BX + SI + DISP16]
	OP_MODRM_MEMORY | OP_MODRM_SEG_DS | OP_MODRM_EA_R0_BX | OP_MODRM_EA_R1_DI | OP_MODRM_EA_DISP16,  // 81: [BX + DI + DISP16]
	OP_MODRM_MEMORY | OP_MODRM_SEG_SS | OP_MODRM_EA_R0_BP | OP_MODRM_EA_R1_SI | OP_MODRM_EA_DISP16,  // 82: [BP + SI + DISP16]
	OP_MODRM_MEMORY | OP_MODRM_SEG_SS | OP_MODRM_EA_R0_BP | OP_MODRM_EA_R1_DI | OP_MODRM_EA_DISP16,  // 83: [BP + DI + DISP16]
	OP_MODRM_MEMORY | OP_MODRM_SEG_DS | OP_MODRM_EA_R0_SI                     | OP_MODRM_EA_DISP16,  // 84: [SI + DISP16]
	OP_MODRM_MEMORY | OP_MODRM_SEG_DS | OP_MODRM_EA_R0_DI                     | OP_MODRM_EA_DISP16,  // 85: [DI + DISP16]
	OP_MODRM_MEMORY | OP_MODRM_SEG_SS | OP_MODRM_EA_R0_BP                     | OP_MODRM_EA_DISP16,  // 86: [BP + DISP16]
	OP_MODRM_MEMORY | OP_MODRM_SEG_DS | OP_MODRM_EA_R0_BX                     | OP_MODRM_EA_DISP16,  // 87: [BX + DISP16]

	OP_MODRM_REGISTER | OP_MODRM_REG_AX_AL | OP_MODRM_REG_ES,  // C0: AX AL ES
	OP_MODRM_REGISTER | OP_MODRM_REG_CX_CL | OP_MODRM_REG_CS,  // C1: CX CL CS
	OP_MODRM_REGISTER | OP_MODRM_REG_DX_DL | OP_MODRM_REG_SS,  // C2: DX DL SS
	OP_MODRM_REGISTER | OP_MODRM_REG_BX_BL | OP_MODRM_REG_DS,  // C3: BX BL DS
	OP_MODRM_REGISTER | OP_MODRM_REG_SP_AH | OP_MODRM_REG_ES,  // C4: SP AH ES
	OP_MODRM_REGISTER | OP_MODRM_REG_BP_CH | OP_MODRM_REG_CS,  // C5: BP CH CS
	OP_MODRM_REGISTER | OP_MODRM_REG_SI_DH | OP_MODRM_REG_SS,  // C6: SI DH SS
	OP_MODRM_REGISTER | OP_MODRM_REG_DI_BH | OP_MODRM_REG_DS   // C7: DI BH DS

};



void i8086_init(CPU)
{

	cpu->regs.zero = 0;

	cpu->regs.ax.w = 0;
	cpu->regs.bx.w = 0;
	cpu->regs.cx.w = 0;
	cpu->regs.dx.w = 0;
	cpu->regs.si.w = 0;
	cpu->regs.di.w = 0;
	cpu->regs.bp.w = 0;
	cpu->regs.sp.w = 0;

	cpu->interrupt.irq     = 0;
	cpu->interrupt.irq_act = false;
	cpu->interrupt.nmi_act = false;
	cpu->interrupt.delay   = false;

	cpu->undef = &op_nop;

	memory_init(&cpu->memory.mem);

	io_init(&cpu->iob);
	io_init(&cpu->iow);


	// Calculate MOD/RM decoding table
	for (int n=0; n < 32; n++) {

		const uint flags = demodrm[n];
		auto       dmdrm = &cpu->microcode.demodrm[n];

		dmdrm->memory = (flags & OP_MODRM_MEMORY) != 0;

		dmdrm->ea_seg    = (flags & OP_MODRM_SEG_SS)? REG_SS: REG_DS;
		dmdrm->ea_disp8  = (flags & OP_MODRM_EA_DISP8)  != 0;
		dmdrm->ea_disp16 = (flags & OP_MODRM_EA_DISP16) != 0;

		dmdrm->ea_reg0 =
			((flags & OP_MODRM_EA_R0) == OP_MODRM_EA_R0_BX)? &cpu->regs.bx.w:
			((flags & OP_MODRM_EA_R0) == OP_MODRM_EA_R0_BP)? &cpu->regs.bp.w:
			((flags & OP_MODRM_EA_R0) == OP_MODRM_EA_R0_SI)? &cpu->regs.si.w:
			((flags & OP_MODRM_EA_R0) == OP_MODRM_EA_R0_DI)? &cpu->regs.di.w: &cpu->regs.zero;

		dmdrm->ea_reg1 =
			((flags & OP_MODRM_EA_R1) == OP_MODRM_EA_R1_BX)? &cpu->regs.bx.w:
			((flags & OP_MODRM_EA_R1) == OP_MODRM_EA_R1_BP)? &cpu->regs.bp.w:
			((flags & OP_MODRM_EA_R1) == OP_MODRM_EA_R1_SI)? &cpu->regs.si.w:
			((flags & OP_MODRM_EA_R1) == OP_MODRM_EA_R1_DI)? &cpu->regs.di.w: &cpu->regs.zero;


		switch (flags & OP_MODRM_REG_GENERIC) {

			case OP_MODRM_REG_AX_AL: dmdrm->regw = &cpu->regs.ax.w; dmdrm->regb = &cpu->regs.ax.l; break;
			case OP_MODRM_REG_BX_BL: dmdrm->regw = &cpu->regs.bx.w; dmdrm->regb = &cpu->regs.bx.l; break;
			case OP_MODRM_REG_CX_CL: dmdrm->regw = &cpu->regs.cx.w; dmdrm->regb = &cpu->regs.cx.l; break;
			case OP_MODRM_REG_DX_DL: dmdrm->regw = &cpu->regs.dx.w; dmdrm->regb = &cpu->regs.dx.l; break;
			case OP_MODRM_REG_SI_DH: dmdrm->regw = &cpu->regs.si.w; dmdrm->regb = &cpu->regs.dx.h; break;
			case OP_MODRM_REG_DI_BH: dmdrm->regw = &cpu->regs.di.w; dmdrm->regb = &cpu->regs.bx.h; break;
			case OP_MODRM_REG_SP_AH: dmdrm->regw = &cpu->regs.sp.w; dmdrm->regb = &cpu->regs.ax.h; break;
			case OP_MODRM_REG_BP_CH: dmdrm->regw = &cpu->regs.bp.w; dmdrm->regb = &cpu->regs.cx.h; break;

			default:
				dmdrm->regw = NULL;
				dmdrm->regb = NULL;
				break;

		}


		switch (flags & OP_MODRM_REG_SEGMENT) {

			case OP_MODRM_REG_CS: dmdrm->regs = (void*)REG_CS; break;
			case OP_MODRM_REG_DS: dmdrm->regs = (void*)REG_DS; break;
			case OP_MODRM_REG_ES: dmdrm->regs = (void*)REG_ES; break;
			case OP_MODRM_REG_SS: dmdrm->regs = (void*)REG_SS; break;

			default:
				dmdrm->regs = NULL;
				break;

		}

	}

	i8086_reset(cpu);

}



void i8086_reset(CPU)
{

	for (int seg=0; seg < 5; seg++)
		memselect(cpu, seg, 0);

	memselect(cpu, REG_CS, 0xffff);
	cpu->regs.ip = 0x0000;

	cpu->flags.c = false;
	cpu->flags.p = false;
	cpu->flags.a = false;
	cpu->flags.z = false;
	cpu->flags.s = false;
	cpu->flags.v = false;
	cpu->flags.d = false;
	cpu->flags.t = false;
	cpu->flags.i = false;

	cpu->insn.fetch     = true;
	cpu->insn.repeat_eq = false;
	cpu->insn.repeat_ne = false;

	cpu->insn.opcode  = 0;
	cpu->insn.modrm   = 0;
	cpu->insn.segment = REG_ZERO;

	cpu->regs.scs = SEGMENT(REG_CS);
	cpu->regs.sip = cpu->regs.ip;

}



int i8086_intrq(CPU, uint nmi, uint irq)
{

	cpu->interrupt.irq     = irq;
	cpu->interrupt.irq_act = nmi == 0;
	cpu->interrupt.nmi_act = nmi != 0;

	return 0;

}



void i8086_tick(CPU)
{

	if (!cpu->interrupt.delay) {

		if (cpu->interrupt.nmi_act) {

			interrupt(cpu, I8086_VECTOR_NMI, cpu->regs.scs, cpu->regs.sip);

			cpu->interrupt.nmi_act = false;
			cpu->insn.fetch        = true;

		} else if (cpu->flags.i && cpu->interrupt.irq_act) {

			interrupt(cpu, cpu->interrupt.irq, cpu->regs.scs, cpu->regs.sip);

			cpu->interrupt.irq_act = false;
			cpu->insn.fetch        = true;

		} else if (cpu->flags.t)
			interrupt(cpu, I8086_VECTOR_SSTEP, cpu->regs.scs, cpu->regs.sip);

	} else
		cpu->interrupt.delay = false;


	if (cpu->insn.fetch) { // Fetch and decode opcode

		const uint op  = LDIPUB();
		const uint opf = opflags[op];

		cpu->insn.opcode = op;
		cpu->insn.modrm  = 0;
		cpu->insn.addr   = 0;

		cpu->insn.op_override = opf & OP_OVRD;
		cpu->insn.op_memory   = false;
		cpu->insn.op_segment  = opf & OP_RSEG;

		cpu->insn.reg0b = NULL;
		cpu->insn.reg0w = NULL;
		cpu->insn.reg1b = NULL;
		cpu->insn.reg1w = NULL;

		if (opf & OP_R02) {

			const auto reg0 = &cpu->microcode.demodrm[(op & 0x07) + 24];

			cpu->insn.reg0w = reg0->regw;
			cpu->insn.reg0b = reg0->regb;

		}


		if (opf & OP_MODRM) { // Fetch and decode MODRM

			cpu->insn.modrm = LDIPUB();

			const uint mod = (cpu->insn.modrm >> 6) & 3;
			const uint reg = (cpu->insn.modrm >> 3) & 7;
			const uint rm  = (cpu->insn.modrm >> 0) & 7;

			const auto demodrm = &cpu->microcode.demodrm[mod * 8 + rm];

			if (opf & OP_GROUP)
				cpu->insn.opcode = 256 + (opf & 0xff) * 8 + reg;

			else {

				const auto reg0 = &cpu->microcode.demodrm[reg + 24];

				cpu->insn.reg0w = cpu->insn.op_segment? reg0->regs: reg0->regw;
				cpu->insn.reg0b = reg0->regb;

			}


			if (demodrm->memory) {

				cpu->insn.addr = *demodrm->ea_reg0 + *demodrm->ea_reg1;

				if      (demodrm->ea_disp8)  cpu->insn.addr += LDIPSB();
				else if (demodrm->ea_disp16) cpu->insn.addr += LDIPUW();

				if (cpu->insn.segment == REG_ZERO)
					cpu->insn.segment = demodrm->ea_seg;

				cpu->insn.op_memory = true;

			} else {

				cpu->insn.reg1b = demodrm->regb;
				cpu->insn.reg1w = demodrm->regw;

			}

		}

		if (cpu->insn.segment == REG_ZERO)
			cpu->insn.segment = REG_DS;

	}

	opcodes[cpu->insn.opcode](cpu);

	if (cpu->insn.fetch && !cpu->insn.op_override) {

		cpu->regs.scs = SEGMENT(REG_CS);
		cpu->regs.sip = cpu->regs.ip;

		cpu->insn.repeat_eq = false;
		cpu->insn.repeat_ne = false;
		cpu->insn.segment   = REG_ZERO;

	}

}



uint i8086_reg_get(i8086 *cpu, uint reg)
{

	switch (reg) {

		case REG_CS: return SEGMENT(REG_CS);
		case REG_DS: return SEGMENT(REG_DS);
		case REG_ES: return SEGMENT(REG_ES);
		case REG_SS: return SEGMENT(REG_SS);

		case REG_IP: return cpu->regs.ip;

		case REG_AX: return cpu->regs.ax.w;
		case REG_BX: return cpu->regs.bx.w;
		case REG_CX: return cpu->regs.cx.w;
		case REG_DX: return cpu->regs.dx.w;

		case REG_AL: return cpu->regs.ax.l;
		case REG_BL: return cpu->regs.bx.l;
		case REG_CL: return cpu->regs.cx.l;
		case REG_DL: return cpu->regs.dx.l;

		case REG_AH: return cpu->regs.ax.h;
		case REG_BH: return cpu->regs.bx.h;
		case REG_CH: return cpu->regs.cx.h;
		case REG_DH: return cpu->regs.dx.h;

		case REG_SI: return cpu->regs.si.w;
		case REG_DI: return cpu->regs.di.w;
		case REG_BP: return cpu->regs.bp.w;
		case REG_SP: return cpu->regs.sp.w;

		case REG_FLAGS: return getf_w(cpu);
		case REG_ZERO:  return cpu->regs.zero;

		case REG_SHADOW_CS: return cpu->regs.scs;
		case REG_SHADOW_IP: return cpu->regs.sip;

		default:
			return 0;

	}

}



void i8086_reg_set(i8086 *cpu, uint reg, uint value)
{

	switch (reg) {

		case REG_CS: memselect(cpu, REG_CS, value); break;
		case REG_DS: memselect(cpu, REG_DS, value); break;
		case REG_ES: memselect(cpu, REG_ES, value); break;
		case REG_SS: memselect(cpu, REG_SS, value); break;

		case REG_IP: cpu->regs.ip = value; break;

		case REG_AX: cpu->regs.ax.w = value; break;
		case REG_BX: cpu->regs.bx.w = value; break;
		case REG_CX: cpu->regs.cx.w = value; break;
		case REG_DX: cpu->regs.dx.w = value; break;

		case REG_AL: cpu->regs.ax.l = value; break;
		case REG_BL: cpu->regs.bx.l = value; break;
		case REG_CL: cpu->regs.cx.l = value; break;
		case REG_DL: cpu->regs.dx.l = value; break;

		case REG_AH: cpu->regs.ax.h = value; break;
		case REG_BH: cpu->regs.bx.h = value; break;
		case REG_CH: cpu->regs.cx.h = value; break;
		case REG_DH: cpu->regs.dx.h = value; break;

		case REG_SI: cpu->regs.si.w = value; break;
		case REG_DI: cpu->regs.di.w = value; break;
		case REG_BP: cpu->regs.bp.w = value; break;
		case REG_SP: cpu->regs.sp.w = value; break;

		case REG_FLAGS: setf_w(cpu, value);     break;
		case REG_ZERO:  cpu->regs.zero = value; break;

		case REG_SHADOW_CS: cpu->regs.scs = value; break;
		case REG_SHADOW_IP: cpu->regs.sip = value; break;

	}

}



static void op_divrmb(CPU)
{

	const ureg tmp = LDEAR1MB();

	if (tmp == 0)
		return interrupt(cpu, I8086_VECTOR_DIVERR, SEGMENT(REG_CS), cpu->regs.ip);

	const ureg div = cpu->regs.ax.w / (u8)tmp;
	const ureg mod = cpu->regs.ax.w % (u8)tmp;

	if (div > 255)
		return interrupt(cpu, I8086_VECTOR_DIVERR, SEGMENT(REG_CS), cpu->regs.ip);

	cpu->regs.ax.l = div;
	cpu->regs.ax.h = mod;

}



static void op_divrmw(CPU)
{

	const ureg tmp = LDEAR1MW();

	if (tmp == 0)
		return interrupt(cpu, I8086_VECTOR_DIVERR, SEGMENT(REG_CS), cpu->regs.ip);

	const ureg num = cpu->regs.dx.w * 65536 + cpu->regs.ax.w;
	const ureg div = num / (u16)tmp;
	const ureg mod = num % (u16)tmp;

	if (div > 65535)
		return interrupt(cpu, I8086_VECTOR_DIVERR, SEGMENT(REG_CS), cpu->regs.ip);

	cpu->regs.ax.w = div;
	cpu->regs.dx.w = mod;

}



static void op_mulrmb(CPU)
{

	ureg tmp = LDEAR1MB();

	cpu->regs.ax.w = cpu->regs.ax.l * tmp;

	cpu->flags.c = (cpu->regs.ax.h != 0);
	cpu->flags.v = cpu->flags.c;

}



static void op_mulrmw(CPU)
{

	ureg tmp = LDEAR1MW();

	tmp *= cpu->regs.ax.w;

	cpu->regs.ax.w = (tmp >> 0)  & 0xffff;
	cpu->regs.dx.w = (tmp >> 16) & 0xffff;

	cpu->flags.c = (cpu->regs.dx.w != 0);
	cpu->flags.v = cpu->flags.c;

}



static void op_idivrmb(CPU)
{

	const i8 tmp = (i8)LDEAR1MB();

	if (tmp == 0)
		return interrupt(cpu, I8086_VECTOR_DIVERR, SEGMENT(REG_CS), cpu->regs.ip);

	const i16 num = (i16)cpu->regs.ax.w;
	const i16 div = num / tmp;
	const i16 mod = num % tmp;

	if (div <= -128 || div > 127)
		return interrupt(cpu, I8086_VECTOR_DIVERR, SEGMENT(REG_CS), cpu->regs.ip);

	cpu->regs.ax.l = div;
	cpu->regs.ax.h = mod;

}



static void op_idivrmw(CPU)
{

	const i16 tmp = (i16)LDEAR1MW();

	if (tmp == 0)
		return interrupt(cpu, I8086_VECTOR_DIVERR, SEGMENT(REG_CS), cpu->regs.ip);

	const i32 num = (i32)((u32)(cpu->regs.dx.w << 16) + cpu->regs.ax.w);
	const i32 div = num / tmp;
	const i32 mod = num % tmp;

	if (div <= -32768 || div > 32767)
		return interrupt(cpu, I8086_VECTOR_DIVERR, SEGMENT(REG_CS), cpu->regs.ip);

	cpu->regs.ax.w = div;
	cpu->regs.dx.w = mod;

}



static void op_imulrmb(CPU)
{

	ureg tmp = (i8)LDEAR1MB();

	tmp *= (i8)cpu->regs.ax.l;

	cpu->regs.ax.w = tmp;

	cpu->flags.c = (i8)cpu->regs.ax.l != tmp;
	cpu->flags.v = cpu->flags.c;

}



static void op_imulrmw(CPU)
{

	ureg tmp = (i16)LDEAR1MW();

	tmp *= (i16)cpu->regs.ax.w;

	cpu->regs.ax.w = (tmp >> 0)  & 0xffff;
	cpu->regs.dx.w = (tmp >> 16) & 0xffff;

	cpu->flags.c = (i16)cpu->regs.ax.w != tmp;
	cpu->flags.v = cpu->flags.c;

}




static void op_aaa(CPU)
{

	if (((cpu->regs.ax.l & 0x0f) > 9) || cpu->flags.a) {

		cpu->flags.a = true;
		cpu->flags.c = true;

		cpu->regs.ax.l += 0x06;
		cpu->regs.ax.h += 0x01;

	} else {

		cpu->flags.a = false;
		cpu->flags.c = false;

	}

	cpu->regs.ax.l &= 0x0f;

}



static void op_aas(CPU)
{

	if (((cpu->regs.ax.l & 0x0f) > 9) || cpu->flags.a) {

		cpu->flags.a = true;
		cpu->flags.c = true;

		cpu->regs.ax.l -= 0x06;
		cpu->regs.ax.h -= 0x01;

	} else {

		cpu->flags.a = false;
		cpu->flags.c = false;

	}

	cpu->regs.ax.l &= 0x0f;

}



static void op_aad(CPU)
{

	const u8 imm = LDIPUB();

        cpu->regs.ax.l = cpu->regs.ax.l + cpu->regs.ax.h * imm;
	cpu->regs.ax.h = 0;

	setf_pzs(cpu, 8, cpu->regs.ax.l);

}



static void op_aam(CPU)
{

	const u8 imm = LDIPUB();

	if (imm == 0) {

		setf_log(cpu, 8, 0);
		return interrupt(cpu, I8086_VECTOR_DIVERR, SEGMENT(REG_CS), cpu->regs.ip);

	}

	const ureg tmp = cpu->regs.ax.l;

	cpu->regs.ax.l = tmp % imm;
	cpu->regs.ax.h = tmp / imm;

	setf_pzs(cpu, 8, cpu->regs.ax.l);

}



static void op_daa(CPU)
{

	const bool carry = cpu->regs.ax.l > (cpu->flags.a? 0x9f: 0x99);

	if (((cpu->regs.ax.l & 0x0f) > 9) || cpu->flags.a) {

		cpu->flags.a = true;
		cpu->regs.ax.l += 0x06;
	}

	if (carry || cpu->flags.c) {

		cpu->flags.c = true;
		cpu->regs.ax.l += 0x60;

	}

	setf_pzs(cpu, 8, cpu->regs.ax.l);

}



static void op_das(CPU)
{

	const bool carry = cpu->regs.ax.l > (cpu->flags.a? 0x9f: 0x99);

	if (((cpu->regs.ax.l & 0x0f) > 9) || cpu->flags.a) {

		cpu->flags.a = true;
		cpu->regs.ax.l -= 0x06;
	}

	if (carry || cpu->flags.c) {

		cpu->flags.c = true;
		cpu->regs.ax.l -= 0x60;

	}

	setf_pzs(cpu, 8, cpu->regs.ax.l);

}



static void op_cmpsb(CPU)
{

	if ((cpu->insn.repeat_eq || cpu->insn.repeat_ne) && (cpu->regs.cx.w == 0))
		return;

	const ureg tmpa = LDMB(cpu->insn.segment, cpu->regs.si.w);
	const ureg tmpb = LDMB(REG_ES,            cpu->regs.di.w);

	sub8(cpu, tmpa, tmpb);
	ADVSIB();
	ADVDIB();

	cpu->insn.fetch =
		cpu->insn.repeat_eq? (--cpu->regs.cx.w == 0) || !cpu->flags.z:
		cpu->insn.repeat_ne? (--cpu->regs.cx.w == 0) || cpu->flags.z:
		true;

}



static void op_cmpsw(CPU)
{

	if ((cpu->insn.repeat_eq || cpu->insn.repeat_ne) && (cpu->regs.cx.w == 0))
		return;

	const ureg tmpa = LDMW(cpu->insn.segment, cpu->regs.si.w);
	const ureg tmpb = LDMW(REG_ES,            cpu->regs.di.w);

	sub16(cpu, tmpa, tmpb);
	ADVSIW();
	ADVDIW();

	cpu->insn.fetch =
		cpu->insn.repeat_eq? (--cpu->regs.cx.w == 0) || !cpu->flags.z:
		cpu->insn.repeat_ne? (--cpu->regs.cx.w == 0) || cpu->flags.z:
		true;

}



static void op_lodsb(CPU)
{

	if ((cpu->insn.repeat_eq || cpu->insn.repeat_ne) && (cpu->regs.cx.w == 0))
		return;

	cpu->regs.ax.l = LDMB(cpu->insn.segment, cpu->regs.si.w);
	ADVSIB();

	cpu->insn.fetch = !(cpu->insn.repeat_eq || cpu->insn.repeat_ne) || (--cpu->regs.cx.w == 0);

}



static void op_lodsw(CPU)
{

	if ((cpu->insn.repeat_eq || cpu->insn.repeat_ne) && (cpu->regs.cx.w == 0))
		return;

	cpu->regs.ax.w = LDMW(cpu->insn.segment, cpu->regs.si.w);

	ADVSIW();

	cpu->insn.fetch = !(cpu->insn.repeat_eq || cpu->insn.repeat_ne) || (--cpu->regs.cx.w == 0);

}



static void op_movsb(CPU)
{

	if ((cpu->insn.repeat_eq || cpu->insn.repeat_ne) && (cpu->regs.cx.w == 0))
		return;

	STMB(REG_ES, cpu->regs.di.w, LDMB(cpu->insn.segment, cpu->regs.si.w));
	ADVSIB();
	ADVDIB();

	cpu->insn.fetch = !(cpu->insn.repeat_eq || cpu->insn.repeat_ne) || (--cpu->regs.cx.w == 0);

}



static void op_movsw(CPU)
{

	if ((cpu->insn.repeat_eq || cpu->insn.repeat_ne) && (cpu->regs.cx.w == 0))
		return;

	STMW(REG_ES, cpu->regs.di.w, LDMW(cpu->insn.segment, cpu->regs.si.w));
	ADVSIW();
	ADVDIW();

	cpu->insn.fetch = !(cpu->insn.repeat_eq || cpu->insn.repeat_ne) || (--cpu->regs.cx.w == 0);

}



static void op_scasb(CPU)
{

	if ((cpu->insn.repeat_eq || cpu->insn.repeat_ne) && (cpu->regs.cx.w == 0))
		return;

	sub8(cpu, cpu->regs.ax.l, LDMB(REG_ES, cpu->regs.di.w));
	ADVDIB();

	if      (cpu->insn.repeat_eq) { cpu->insn.fetch = (--cpu->regs.cx.w == 0) || !cpu->flags.z; }
	else if (cpu->insn.repeat_ne) { cpu->insn.fetch = (--cpu->regs.cx.w == 0) ||  cpu->flags.z; }

}



static void op_scasw(CPU)
{

	if ((cpu->insn.repeat_eq || cpu->insn.repeat_ne) && (cpu->regs.cx.w == 0))
		return;

	sub16(cpu, cpu->regs.ax.w, LDMW(REG_ES, cpu->regs.di.w));
	ADVDIW();

	if      (cpu->insn.repeat_eq) { cpu->insn.fetch = (--cpu->regs.cx.w == 0) || !cpu->flags.z; }
	else if (cpu->insn.repeat_ne) { cpu->insn.fetch = (--cpu->regs.cx.w == 0) ||  cpu->flags.z; }

}



static void op_stosb(CPU)
{

	if ((cpu->insn.repeat_eq || cpu->insn.repeat_ne) && (cpu->regs.cx.w == 0))
		return;

	STMB(REG_ES, cpu->regs.di.w, cpu->regs.ax.l);
	ADVDIB();

	cpu->insn.fetch = !(cpu->insn.repeat_eq || cpu->insn.repeat_ne) || (--cpu->regs.cx.w == 0);

}



static void op_stosw(CPU)
{

	if ((cpu->insn.repeat_eq || cpu->insn.repeat_ne) && (cpu->regs.cx.w == 0))
		return;

	STMW(REG_ES, cpu->regs.di.w, cpu->regs.ax.w);
	ADVDIW();

	cpu->insn.fetch = !(cpu->insn.repeat_eq || cpu->insn.repeat_ne) || (--cpu->regs.cx.w == 0);

}

