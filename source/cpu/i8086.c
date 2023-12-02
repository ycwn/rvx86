

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <Zydis/Zydis.h>

#include "core/types.h"
#include "core/debug.h"

#include "core/bus.h"
#include "core/wire.h"

#include "cpu/i8086.h"



#define CPU  struct i8086 *cpu

#define STAGE(flag, cond) \
	if (!flag && !(cond)) return; \
	if (!flag && (flag = true))


typedef u32 aluu;
typedef i32 alui;



static u8  *get_r8( CPU, uint r);
static u16 *get_r16(CPU, uint r);
static u16 *get_rs( CPU, uint r);


static inline bool zero(  aluu x, uint m)                 { return (x & (2 * m - 1)) == 0; }
static inline bool sign(  aluu x, uint m)                 { return (x & m) != 0; }
static inline bool vflow( aluu x, aluu a, aluu b, uint m) { return sign(a, m) == sign(b, m) && sign(b, m) != sign(x, m); }
static inline bool carry( aluu x, uint m)                 { return (x & (2 * m - 1)) != x; }
static inline bool parity(aluu x) {
	x = x ^ (x >> 1);
	x = x ^ (x >> 2);
	x = x ^ (x >> 4);
	return !(x & 1);
}

//static inline bool svflow(aluu a, uint m) { return sign(a, m) != sign(a, m >> 1); }

static inline aluu alunumop(CPU, uint m, aluu x, aluu a, aluu b) {
	cpu->flags.c = carry(x, m);
	cpu->flags.v = vflow(x, a, b, m);
	cpu->flags.p = parity(x);
	cpu->flags.a = false;  // FIXME: Aux carry not implemented!
	cpu->flags.z = zero(x, m);
	cpu->flags.s = sign(x, m);
	return x;
}

static inline aluu aluincop(CPU, uint m, aluu x, aluu a, aluu b) {
	cpu->flags.v = vflow(x, a, b, m);
	cpu->flags.p = parity(x);
	cpu->flags.a = false;  // FIXME: Aux carry not implemented!
	cpu->flags.z = zero(x, m);
	cpu->flags.s = sign(x, m);
	return x;
}

static inline aluu alulogop(CPU, uint m, aluu x)
{
	cpu->flags.c = false;
	cpu->flags.v = false;
	cpu->flags.p = parity(x);
	cpu->flags.a = false;  // FIXME: Aux carry not implemented!
	cpu->flags.z = zero(x, m);
	cpu->flags.s = sign(x, m);
	return x;
}

static inline aluu alushrop(CPU, uint m, aluu x, aluu a, uint c) {
	cpu->flags.c = sign(a, c);
	cpu->flags.v = vflow(x, a, a, m);
	cpu->flags.p = parity(x);
	cpu->flags.a = false;  // FIXME: Aux carry not implemented!
	cpu->flags.z = zero(x, m);
	cpu->flags.s = sign(x, m);
	return x;
}

static inline aluu alurorop(CPU, uint m, aluu x, aluu a, uint c) {
	cpu->flags.c = sign(a, c);
	cpu->flags.v = vflow(x, a, a, m);
	cpu->flags.a = false;  // FIXME: Aux carry not implemented!
	return x;
}


static inline aluu add8( CPU, aluu a, aluu b) { return alunumop(cpu, 0x80,   a + b,                a, b); }
static inline aluu add16(CPU, aluu a, aluu b) { return alunumop(cpu, 0x8000, a + b,                a, b); }
static inline aluu adc8( CPU, aluu a, aluu b) { return alunumop(cpu, 0x80,   a + b + cpu->flags.c, a, b); }
static inline aluu adc16(CPU, aluu a, aluu b) { return alunumop(cpu, 0x8000, a + b + cpu->flags.c, a, b); }

static inline aluu sub8( CPU, aluu a, aluu b) { return alunumop(cpu, 0x80,   a - b,                a, ~b); }
static inline aluu sub16(CPU, aluu a, aluu b) { return alunumop(cpu, 0x8000, a - b,                a, ~b); }
static inline aluu sbb8( CPU, aluu a, aluu b) { return alunumop(cpu, 0x80,   a - b - cpu->flags.c, a, ~b); }
static inline aluu sbb16(CPU, aluu a, aluu b) { return alunumop(cpu, 0x8000, a - b - cpu->flags.c, a, ~b); }

static inline aluu inc8( CPU, aluu a) { return aluincop(cpu, 0x80,   a + 1, a, 1); }
static inline aluu inc16(CPU, aluu a) { return aluincop(cpu, 0x8000, a + 1, a, 1); }
static inline aluu dec8( CPU, aluu a) { return aluincop(cpu, 0x80,   a - 1, a, ~1); }
static inline aluu dec16(CPU, aluu a) { return aluincop(cpu, 0x8000, a - 1, a, ~1); }

static inline aluu and8(CPU, aluu a, aluu b) { return alulogop(cpu, 0x80, a & b); }
static inline aluu ior8(CPU, aluu a, aluu b) { return alulogop(cpu, 0x80, a | b); }
static inline aluu xor8(CPU, aluu a, aluu b) { return alulogop(cpu, 0x80, a ^ b); }
static inline aluu not8(CPU, aluu a)         { return alulogop(cpu, 0x80, ~a); }


static inline aluu shrb(aluu a, uint b)         { return (u8)a >> b; }
static inline aluu shlb(aluu a, uint b)         { return (u8)a << b; }
static inline aluu sarb(aluu a, uint b)         { return (u8)((i8)a >> b); }
static inline aluu salb(aluu a, uint b)         { return (u8)((i8)a << b); }
static inline aluu rorb(aluu a, uint b)         { return (u8)((a >> b) | (a << 8-b)); }
static inline aluu rolb(aluu a, uint b)         { return (u8)((a << b) | (a >> 8-b)); }
static inline aluu rcrb(aluu a, uint b, bool c) { return (u8)((a >> b) | (a << 9-b) | (c << 8-b)); }
static inline aluu rclb(aluu a, uint b, bool c) { return (u8)((a << b) | (a >> 9-b) | (c << b-1)); }

static inline aluu shrw(aluu a, uint b)         { return (u16)a >> b; }
static inline aluu shlw(aluu a, uint b)         { return (u16)a << b; }
static inline aluu sarw(aluu a, uint b)         { return (u16)((i16)a >> b); }
static inline aluu salw(aluu a, uint b)         { return (u16)((i16)a << b); }
static inline aluu rorw(aluu a, uint b)         { return (u16)((a >> b) | (a << 16-b)); }
static inline aluu rolw(aluu a, uint b)         { return (u16)((a << b) | (a >> 16-b)); }
static inline aluu rcrw(aluu a, uint b, bool c) { return (u16)((a >> b) | (a << 17-b) | (c << 16-b)); }
static inline aluu rclw(aluu a, uint b, bool c) { return (u16)((a << b) | (a >> 17-b) | (c << b-1)); }

static inline uint max8( uint c) { return (c < 8)?  c: 8; }
static inline uint max16(uint c) { return (c < 16)? c: 16; }

static inline aluu shr8(CPU, aluu a, uint b) { return alushrop(cpu, 0x80, shrb(a, max8(b)), a, 0x01 << max8(b-1)); }
static inline aluu shl8(CPU, aluu a, uint b) { return alushrop(cpu, 0x80, shlb(a, max8(b)), a, 0x80 >> max8(b-1)); }
static inline aluu sar8(CPU, aluu a, uint b) { return alushrop(cpu, 0x80, sarb(a, max8(b)), a, 0x01 << max8(b)-1); }
static inline aluu sal8(CPU, aluu a, uint b) { return alushrop(cpu, 0x80, salb(a, max8(b)), a, 0x80 >> max8(b)-1); }
static inline aluu ror8(CPU, aluu a, uint b) { return alurorop(cpu, 0x80, rorb(a, b & 7), a, 0x01 << ((b-1)&7)); }
static inline aluu rol8(CPU, aluu a, uint b) { return alurorop(cpu, 0x80, rolb(a, b & 7), a, 0x80 >> ((b-1)&7)); }
static inline aluu rcr8(CPU, aluu a, uint b) { return alurorop(cpu, 0x80, rcrb(a, b, cpu->flags.c), a, 0x01 << b-1); }
static inline aluu rcl8(CPU, aluu a, uint b) { return alurorop(cpu, 0x80, rclb(a, b, cpu->flags.c), a, 0x80 >> b-1); }

static inline aluu and16(CPU, aluu a, u16 b) { return alulogop(cpu, 0x8000, a & b); }
static inline aluu ior16(CPU, aluu a, u16 b) { return alulogop(cpu, 0x8000, a | b); }
static inline aluu xor16(CPU, aluu a, u16 b) { return alulogop(cpu, 0x8000, a ^ b); }
static inline aluu not16(CPU, aluu a)        { return alulogop(cpu, 0x8000, ~a); }

static inline aluu shr16(CPU, aluu a, uint b) { return alushrop(cpu, 0x8000, shrw(a, max16(b)), a, 0x0001 << max16(b-1)); }
static inline aluu shl16(CPU, aluu a, uint b) { return alushrop(cpu, 0x8000, shlw(a, max16(b)), a, 0x8000 >> max16(b-1)); }
static inline aluu sar16(CPU, aluu a, uint b) { return alushrop(cpu, 0x8000, sarw(a, max16(b)), a, 0x0001 << max16(b)-1); }
static inline aluu sal16(CPU, aluu a, uint b) { return alushrop(cpu, 0x8000, salw(a, max16(b)), a, 0x8000 >> max16(b)-1); }
static inline aluu ror16(CPU, aluu a, uint b) { return alurorop(cpu, 0x8000, rorw(a, b & 15), a, 0x0001 << ((b-1) & 15)); }
static inline aluu rol16(CPU, aluu a, uint b) { return alurorop(cpu, 0x8000, rolw(a, b & 15), a, 0x8000 >> ((b-1) & 15)); }
static inline aluu rcr16(CPU, aluu a, uint b) { return alurorop(cpu, 0x8000, rcrw(a, b, cpu->flags.c), a, 0x0001 << b-1); }
static inline aluu rcl16(CPU, aluu a, uint b) { return alurorop(cpu, 0x8000, rclw(a, b, cpu->flags.c), a, 0x8000 >> b-1); }



static inline void decsp8(CPU)  { cpu->regs.sp.w--; }
static inline void decsp16(CPU) { cpu->regs.sp.w -= 2; }


static inline void pushx8(CPU, aluu v) {
	bus_write8(&cpu->mem, cpu->regs.ss * 16 + cpu->regs.sp.w, v);
}


static inline void pushx16(CPU, aluu v) {
	bus_write16(&cpu->mem, cpu->regs.ss * 16 + cpu->regs.sp.w, v);
}


static inline uint pop8(CPU) {
	uint v = bus_read8(&cpu->mem, cpu->regs.ss * 16 + cpu->regs.sp.w);
	cpu->regs.sp.w++;
	return v;
}


static inline uint pop16(CPU) {
	uint v = bus_read16(&cpu->mem, cpu->regs.ss * 16 + cpu->regs.sp.w);
	cpu->regs.sp.w += 2;
	return v;
}


static void op_adcaib(CPU);  // Arithmetic
static void op_adcaiw(CPU);
static void op_adcrmib(CPU);
static void op_adcrmiw(CPU);
static void op_adcrmb(CPU);
static void op_adcrmw(CPU);
static void op_addaib(CPU);
static void op_addaiw(CPU);
static void op_addrmib(CPU);
static void op_addrmiw(CPU);
static void op_addrmb(CPU);
static void op_addrmw(CPU);
static void op_cmpaib(CPU);
static void op_cmprib(CPU);
static void op_cmpriw(CPU);
static void op_cmpaiw(CPU);
static void op_cmprmib(CPU);
static void op_cmprmiw(CPU);
static void op_cmprmb(CPU);
static void op_cmprmw(CPU);
static void op_decrmb(CPU);
static void op_decrmw(CPU);
static void op_decrw(CPU);
static void op_divrmb(CPU);
static void op_divrmw(CPU);
static void op_idivrmb(CPU);
static void op_idivrmw(CPU);
static void op_imulrmb(CPU);
static void op_imulrmw(CPU);
static void op_incrmb(CPU);
static void op_incrmw(CPU);
static void op_incrw(CPU);
static void op_mulrmb(CPU);
static void op_mulrmw(CPU);
static void op_negrmb(CPU);
static void op_negrmw(CPU);
static void op_sbbaib(CPU);
static void op_sbbaiw(CPU);
static void op_sbbrmib(CPU);
static void op_sbbrmiw(CPU);
static void op_sbbrmb(CPU);
static void op_sbbrmw(CPU);
static void op_subaib(CPU);
static void op_subaiw(CPU);
static void op_subrib(CPU);
static void op_subriw(CPU);
static void op_subrmb(CPU);
static void op_subrmw(CPU);

static void op_andaib(CPU);  // Logic
static void op_andaiw(CPU);
static void op_andrib(CPU);
static void op_andriw(CPU);
static void op_andrmb(CPU);
static void op_andrmw(CPU);
static void op_notrmb(CPU);
static void op_notrmw(CPU);
static void op_ioraib(CPU);
static void op_ioraiw(CPU);
static void op_iorrib(CPU);
static void op_iorriw(CPU);
static void op_iorrmb(CPU);
static void op_iorrmw(CPU);
static void op_tstaib(CPU);
static void op_tstaiw(CPU);
static void op_tstrib(CPU);
static void op_tstriw(CPU);
static void op_tstrmb(CPU);
static void op_tstrmw(CPU);
static void op_xoraib(CPU);
static void op_xoraiw(CPU);
static void op_xorrib(CPU);
static void op_xorriw(CPU);
static void op_xorrmb(CPU);
static void op_xorrmw(CPU);

static void op_rclb1(CPU);    // Shift and rotation
static void op_rclbr(CPU);
static void op_rclw1(CPU);
static void op_rclwr(CPU);
static void op_rcrb1(CPU);
static void op_rcrbr(CPU);
static void op_rcrw1(CPU);
static void op_rcrwr(CPU);
static void op_rolb1(CPU);
static void op_rolbr(CPU);
static void op_rolw1(CPU);
static void op_rolwr(CPU);
static void op_rorb1(CPU);
static void op_rorbr(CPU);
static void op_rorw1(CPU);
static void op_rorwr(CPU);
static void op_salb1(CPU);
static void op_salbr(CPU);
static void op_salw1(CPU);
static void op_salwr(CPU);
static void op_sarb1(CPU);
static void op_sarbr(CPU);
static void op_sarw1(CPU);
static void op_sarwr(CPU);
static void op_shlb1(CPU);
static void op_shlbr(CPU);
static void op_shlw1(CPU);
static void op_shlwr(CPU);
static void op_shrb1(CPU);
static void op_shrbr(CPU);
static void op_shrw1(CPU);
static void op_shrwr(CPU);

static void op_aaa(CPU);      // Conversions
static void op_aad(CPU);
static void op_aas(CPU);
static void op_aam(CPU);
static void op_cbw(CPU);
static void op_cwd(CPU);
static void op_daa(CPU);
static void op_das(CPU);

static void op_lahf(CPU);     // Load and store
static void op_ldsr(CPU);
static void op_lesr(CPU);
static void op_lear(CPU);
static void op_movamb(CPU);
static void op_movamw(CPU);
static void op_movrib(CPU);
static void op_movriw(CPU);
static void op_movrmib(CPU);
static void op_movrmiw(CPU);
static void op_movrrmb(CPU);
static void op_movrrmw(CPU);
static void op_movseg(CPU);
static void op_sahf(CPU);
static void op_xcharw(CPU);
static void op_xchrmb(CPU);
static void op_xchrmw(CPU);
static void op_xlatab(CPU);

static void op_popcs( CPU);   // Stack
static void op_popds( CPU);
static void op_popes( CPU);
static void op_popss( CPU);
static void op_popf(  CPU);
static void op_poprmw(CPU);
static void op_poprw( CPU);
static void op_pushcs(CPU);
static void op_pushds(CPU);
static void op_pushes(CPU);
static void op_pushss(CPU);
static void op_pushsp(CPU);
static void op_pushf( CPU);
static void op_pushrmw(CPU);
static void op_pushrw(CPU);

static void op_callf(CPU);   // Flow control
static void op_calln(CPU);
static void op_callfrm(CPU);
static void op_callnrm(CPU);
static void op_int3(CPU);
static void op_intib(CPU);
static void op_into(CPU);
static void op_iret(CPU);
static void op_jcbe(CPU);
static void op_jcle(CPU);
static void op_jcc(CPU);
static void op_jcl(CPU);
static void op_jco(CPU);
static void op_jcp(CPU);
static void op_jcs(CPU);
static void op_jcz(CPU);
static void op_jcnbe(CPU);
static void op_jcnle(CPU);
static void op_jcnc(CPU);
static void op_jcnl(CPU);
static void op_jcno(CPU);
static void op_jcnp(CPU);
static void op_jcns(CPU);
static void op_jcnz(CPU);
static void op_jcxzr(CPU);
static void op_jmpf(CPU);
static void op_jmpfrm(CPU);
static void op_jmpn(CPU);
static void op_jmpnrm(CPU);
static void op_loopnzr(CPU);
static void op_loopzr(CPU);
static void op_loopr(CPU);
static void op_retf(CPU);
static void op_retn(CPU);

static void op_inib(CPU);    // I/O
static void op_iniw(CPU);
static void op_inrb(CPU);
static void op_inrw(CPU);
static void op_outib(CPU);
static void op_outiw(CPU);
static void op_outrb(CPU);
static void op_outrw(CPU);

static void op_segcs(CPU);   // Overrides
static void op_segds(CPU);
static void op_seges(CPU);
static void op_segss(CPU);
static void op_lock(CPU);
static void op_repnz(CPU);
static void op_repz(CPU);
static void op_wait(CPU);

static void op_clc(CPU);     // Flags
static void op_cld(CPU);
static void op_cli(CPU);
static void op_cmc(CPU);
static void op_stc(CPU);
static void op_std(CPU);
static void op_sti(CPU);

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

static void op_fpu(CPU);     // FPU

static void op_undef(CPU);   // Invalid


typedef void (*opcode)(CPU);

enum {

	OP_MODRM  = 1 << 8,  // Requires MODRM
	OP_W8     = 0 << 9,  // Is 8-bit operation
	OP_W16    = 1 << 9,  // Is 16-bit operation
	OP_FWD    = 0 << 10, // Operands are not reversed
	OP_REV    = 1 << 10, // Operands are reversed
	OP_RSEG   = 1 << 11, // REG in MODRM is segment register
	OP_GROUP  = 1 << 12, // Group opcode, REG field of MODRM is operation
	OP_EAIMM0 = 1 << 13, // Use first immediate operand to compute effective address
	OP_EASEG  = 1 << 14, // Use segment to compute effective address
	OP_NEASEG = 1 << 15, // Don't use segment to compute effective address
	OP_OVRD   = 1 << 16, // Opcode is override, keep flags
	OP_R02    = 1 << 17, // Lowest 3 bits of opcode is register
	OP_I0B    = 1 << 18, // Requires first sign-extended 8bit immediate operand
	OP_I0W    = 1 << 19, // Requires first 16bit immediate operand
	OP_I1B    = 1 << 20, // Requires first sign-extended 8bit immediate operand
	OP_I1W    = 1 << 21, // Requires first 16bit immediate operand

	OP_RMBF  = OP_MODRM | OP_W8  | OP_FWD,
	OP_RMWF  = OP_MODRM | OP_W16 | OP_FWD,
	OP_RMBR  = OP_MODRM | OP_W8  | OP_REV,
	OP_RMWR  = OP_MODRM | OP_W16 | OP_REV,
	OP_R02B  = OP_R02   | OP_W8,
	OP_R02W  = OP_R02   | OP_W16,
	OP_ADRF  = OP_I0W   | OP_I1W,
	OP_ADRN  = OP_I0W,
	OP_ADRR  = OP_I0B,

	OP_RIBBF = OP_I0B | OP_W8 | OP_FWD,
	OP_RIWBF = OP_I0W | OP_W8 | OP_FWD,
	OP_RIBBR = OP_I0B | OP_W8 | OP_REV,
	OP_RIWBR = OP_I0W | OP_W8 | OP_REV,

	OP_RIBWF = OP_I0B | OP_W16 | OP_FWD,
	OP_RIWWF = OP_I0W | OP_W16 | OP_FWD,
	OP_RIBWR = OP_I0B | OP_W16 | OP_REV,
	OP_RIWWR = OP_I0W | OP_W16 | OP_REV,

	OP_RMIBF = OP_MODRM | OP_I1B | OP_W8  | OP_FWD,
	OP_RMIWF = OP_MODRM | OP_I1W | OP_W16 | OP_FWD,
	OP_RMIBR = OP_MODRM | OP_I1B | OP_W8  | OP_REV,
	OP_RMIWR = OP_MODRM | OP_I1W | OP_W16 | OP_REV,

	OP_R2IB = OP_R02   | OP_W8  | OP_I0B,
	OP_R2IW = OP_R02   | OP_W16 | OP_I0W,
	OP_SMBF = OP_MODRM | OP_W16 | OP_RSEG | OP_FWD,
	OP_SMBR = OP_MODRM | OP_W16 | OP_RSEG | OP_REV,

	OP_RANBF = OP_EASEG | OP_EAIMM0 | OP_I0W | OP_W8  | OP_FWD,
	OP_RANBR = OP_EASEG | OP_EAIMM0 | OP_I0W | OP_W8  | OP_REV,
	OP_RANWF = OP_EASEG | OP_EAIMM0 | OP_I0W | OP_W16 | OP_FWD,
	OP_RANWR = OP_EASEG | OP_EAIMM0 | OP_I0W | OP_W16 | OP_REV,

	OP_G0  =  0 | OP_RMBF | OP_GROUP | OP_I1B,
	OP_G1  =  1 | OP_RMWF | OP_GROUP | OP_I1W,
	OP_G2  =  2 | OP_RMBF | OP_GROUP | OP_I1B,
	OP_G3  =  3 | OP_RMWF | OP_GROUP | OP_I1B,
	OP_G4  =  4 | OP_RMBF | OP_GROUP,
	OP_G5  =  5 | OP_RMWF | OP_GROUP,
	OP_G6  =  6 | OP_RMBF | OP_GROUP,
	OP_G7  =  7 | OP_RMWF | OP_GROUP,
	OP_G8  =  8 | OP_RMBF | OP_GROUP,
	OP_G9  =  9 | OP_RMWF | OP_GROUP,
	OP_G10 = 10 | OP_RMBF | OP_GROUP,
	OP_G11 = 11 | OP_RMWF | OP_GROUP

};


static const opcode opcodes[352] = {

// Main opcodes
// 0x00
	&op_addrmb,  &op_addrmw,  &op_addrmb,  &op_addrmw,  &op_addaib,  &op_addaiw,  &op_pushes,  &op_popes,
	&op_iorrmb,  &op_iorrmw,  &op_iorrmb,  &op_iorrmw,  &op_ioraib,  &op_ioraiw,  &op_pushcs,  &op_popcs,
	&op_adcrmb,  &op_adcrmw,  &op_adcrmb,  &op_adcrmw,  &op_adcaib,  &op_adcaiw,  &op_pushss,  &op_popss,
	&op_sbbrmb,  &op_sbbrmw,  &op_sbbrmb,  &op_sbbrmw,  &op_sbbaib,  &op_sbbaiw,  &op_pushds,  &op_popds,
// 0x20
	&op_andrmb,  &op_andrmw,  &op_andrmb,  &op_andrmw,  &op_andaib,  &op_andaiw,  &op_seges,   &op_daa,
	&op_subrmb,  &op_subrmw,  &op_subrmb,  &op_subrmw,  &op_subaib,  &op_subaiw,  &op_segcs,   &op_das,
	&op_xorrmb,  &op_xorrmw,  &op_xorrmb,  &op_xorrmw,  &op_xoraib,  &op_xoraiw,  &op_segss,   &op_aaa,
	&op_cmprmb,  &op_cmprmw,  &op_cmprmb,  &op_cmprmw,  &op_cmpaib,  &op_cmpaiw,  &op_segds,   &op_aas,
// 0x40
	&op_incrw,   &op_incrw,   &op_incrw,   &op_incrw,   &op_incrw,   &op_incrw,   &op_incrw,   &op_incrw,
	&op_decrw,   &op_decrw,   &op_decrw,   &op_decrw,   &op_decrw,   &op_decrw,   &op_decrw,   &op_decrw,
	&op_pushrw,  &op_pushrw,  &op_pushrw,  &op_pushrw,  &op_pushsp,  &op_pushrw,  &op_pushrw,  &op_pushrw,
	&op_poprw,   &op_poprw,   &op_poprw,   &op_poprw,   &op_poprw,   &op_poprw,   &op_poprw,   &op_poprw,
// 0x60
	&op_undef,   &op_undef,   &op_undef,   &op_undef,   &op_undef,   &op_undef,   &op_undef,   &op_undef,
	&op_undef,   &op_undef,   &op_undef,   &op_undef,   &op_undef,   &op_undef,   &op_undef,   &op_undef,
	&op_jco,     &op_jcno,    &op_jcc,     &op_jcnc,    &op_jcz,     &op_jcnz,    &op_jcbe,    &op_jcnbe,
	&op_jcs,     &op_jcns,    &op_jcp,     &op_jcnp,    &op_jcl,     &op_jcnl,    &op_jcle,    &op_jcnle,
// 0x80
	&op_undef,   &op_undef,   &op_undef,   &op_undef,   &op_tstrmb,  &op_tstrmw,  &op_xchrmb,  &op_xchrmw,
	&op_movrrmb, &op_movrrmw, &op_movrrmb, &op_movrrmw, &op_movseg,  &op_lear,    &op_movseg,  &op_poprmw,
	&op_xcharw,  &op_xcharw,  &op_xcharw,  &op_xcharw,  &op_xcharw,  &op_xcharw,  &op_xcharw,  &op_xcharw,
	&op_cbw,     &op_cwd,     &op_callf,   &op_wait,    &op_pushf,   &op_popf,    &op_sahf,    &op_lahf,
// 0xa0
	&op_movamb,  &op_movamw,  &op_movamb,  &op_movamw,  &op_movsb,   &op_movsw,   &op_cmpsb,   &op_cmpsw,
	&op_tstaib,  &op_tstaiw,  &op_stosb,   &op_stosw,   &op_lodsb,   &op_lodsw,   &op_scasb,   &op_scasw,
	&op_movrib,  &op_movrib,  &op_movrib,  &op_movrib,  &op_movrib,  &op_movrib,  &op_movrib,  &op_movrib,
	&op_movriw,  &op_movriw,  &op_movriw,  &op_movriw,  &op_movriw,  &op_movriw,  &op_movriw,  &op_movriw,
// 0xc0
	&op_retn,    &op_retn,    &op_retn,    &op_retn,    &op_lesr,    &op_ldsr,    &op_movrmib, &op_movrmiw,
	&op_retf,    &op_retf,    &op_retf,    &op_retf,    &op_int3,    &op_intib,   &op_into,    &op_iret,
	&op_undef,   &op_undef,   &op_undef,   &op_undef,   &op_aam,     &op_aad,     &op_undef,   &op_xlatab,
	&op_fpu,     &op_fpu,     &op_fpu,     &op_fpu,     &op_fpu,     &op_fpu,     &op_fpu,     &op_fpu,
// 0xe0
	&op_loopnzr, &op_loopzr,  &op_loopr,   &op_jcxzr,   &op_inib,    &op_iniw,    &op_outib,   &op_outiw,
	&op_calln,   &op_jmpn,    &op_jmpf,    &op_jmpn,    &op_inrb,    &op_inrw,    &op_outrb,   &op_outrw,
	&op_lock,    &op_undef,   &op_repnz,   &op_repz,    &op_undef,   &op_cmc,     &op_undef,   &op_undef,
	&op_clc,     &op_stc,     &op_cli,     &op_sti,     &op_cld,     &op_std,     &op_undef,   &op_undef,

// Group opcodes
// 0x100
	&op_addrmib, &op_iorrib,  &op_adcrmib, &op_sbbrmib, &op_andrib,  &op_subrib,  &op_xorrib,  &op_cmprmib,  // 0:  0x80
	&op_addrmiw, &op_iorriw,  &op_adcrmiw, &op_sbbrmiw, &op_andriw,  &op_subriw,  &op_xorriw,  &op_cmprmiw,  // 1:  0x81
	&op_addrmib, &op_iorrib,  &op_adcrmib, &op_sbbrmib, &op_andrib,  &op_subrib,  &op_xorrib,  &op_cmprmib,  // 2:  0x82
	&op_addrmiw, &op_iorriw,  &op_adcrmiw, &op_sbbrmiw, &op_andriw,  &op_subriw,  &op_xorriw,  &op_cmprmiw,  // 3:  0x83
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
	OP_RMBR,  OP_RMWR,  OP_RMBF,  OP_RMWF,  OP_RIBBF, OP_RIWWF, 0,        0,
	OP_RMBR,  OP_RMWR,  OP_RMBF,  OP_RMWF,  OP_RIBBF, OP_RIWWF, 0,        0,
	OP_RMBR,  OP_RMWR,  OP_RMBF,  OP_RMWF,  OP_RIBBF, OP_RIWWF, 0,        0,
	OP_RMBR,  OP_RMWR,  OP_RMBF,  OP_RMWF,  OP_RIBBF, OP_RIWWF, 0,        0,
// 0x20
	OP_RMBR,  OP_RMWR,  OP_RMBF,  OP_RMWF,  OP_RIBBF, OP_RIWWF, OP_OVRD,  0,
	OP_RMBR,  OP_RMWR,  OP_RMBF,  OP_RMWF,  OP_RIBBF, OP_RIWWF, OP_OVRD,  0,
	OP_RMBR,  OP_RMWR,  OP_RMBF,  OP_RMWF,  OP_RIBBF, OP_RIWWF, OP_OVRD,  0,
	OP_RMBR,  OP_RMWR,  OP_RMBF,  OP_RMWF,  OP_RIBBF, OP_RIWWF, OP_OVRD,  0,
// 0x40
	OP_R02W,  OP_R02W,  OP_R02W,  OP_R02W,  OP_R02W,  OP_R02W,  OP_R02W,  OP_R02W,
	OP_R02W,  OP_R02W,  OP_R02W,  OP_R02W,  OP_R02W,  OP_R02W,  OP_R02W,  OP_R02W,
	OP_R02W,  OP_R02W,  OP_R02W,  OP_R02W,  OP_R02W,  OP_R02W,  OP_R02W,  OP_R02W,
	OP_R02W,  OP_R02W,  OP_R02W,  OP_R02W,  OP_R02W,  OP_R02W,  OP_R02W,  OP_R02W,
// 0x60
	OP_ADRR,  OP_ADRR,  OP_ADRR,  OP_ADRR,  OP_ADRR,  OP_ADRR,  OP_ADRR,  OP_ADRR,
	OP_ADRR,  OP_ADRR,  OP_ADRR,  OP_ADRR,  OP_ADRR,  OP_ADRR,  OP_ADRR,  OP_ADRR,
	OP_ADRR,  OP_ADRR,  OP_ADRR,  OP_ADRR,  OP_ADRR,  OP_ADRR,  OP_ADRR,  OP_ADRR,
	OP_ADRR,  OP_ADRR,  OP_ADRR,  OP_ADRR,  OP_ADRR,  OP_ADRR,  OP_ADRR,  OP_ADRR,
// 0x80
	OP_G0,    OP_G1,    OP_G2,    OP_G3,    OP_RMBF,  OP_RMWF,  OP_RMBF,  OP_RMWF,
	OP_RMBR,  OP_RMWR,  OP_RMBF,  OP_RMWF,  OP_SMBR,  OP_RMWF | OP_NEASEG,  OP_SMBF,  OP_RMWF,
	OP_R02W,  OP_R02W,  OP_R02W,  OP_R02W,  OP_R02W,  OP_R02W,  OP_R02W,  OP_R02W,
	0,        0,        OP_ADRF,  0,        0,        0,        0,        0,
// 0xa0
	OP_RANBF, OP_RANWF, OP_RANBR, OP_RANWR, OP_W8,    OP_W16,   OP_W8,    OP_W16,
	OP_RIBBF, OP_RIWWF, OP_W8,    OP_W16,   OP_W8,    OP_W16,   0,        0,
	OP_R2IB,  OP_R2IB,  OP_R2IB,  OP_R2IB,  OP_R2IB,  OP_R2IB,  OP_R2IB,  OP_R2IB,
	OP_R2IW,  OP_R2IW,  OP_R2IW,  OP_R2IW,  OP_R2IW,  OP_R2IW,  OP_R2IW,  OP_R2IW,
// 0xc0
	OP_I0W,   0,        OP_I0W,   0,        OP_RMWF,  OP_RMWF,  OP_RMIBF, OP_RMIWF,
	OP_I0W,   0,        OP_I0W,   0,        0,        OP_I0B,   0,        0,
	OP_G4,    OP_G5,    OP_G6,    OP_G7,    OP_RIBBF, OP_RIBBF, 0,        0,
	OP_MODRM, OP_MODRM, OP_MODRM, OP_MODRM, OP_MODRM, OP_MODRM, OP_MODRM, OP_MODRM,
// 0xe0
	OP_ADRR,  OP_ADRR,  OP_ADRR,  OP_ADRR,  OP_RIBBF, OP_RIBWF, OP_RIBBF, OP_RIBWF,
	OP_ADRN,  OP_ADRN,  OP_ADRF,  OP_ADRR,  OP_W8,    OP_W16,   OP_W8,    OP_W16,
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



void i8086_init(CPU)
{

	bus_init(&cpu->io);
	bus_init(&cpu->mem);

	cpu->regs.ds = 0x0000;
	cpu->regs.es = 0x0000;
	cpu->regs.ss = 0x0000;

	cpu->regs.bp.w = 0x0000;
	cpu->regs.sp.w = 0x0000;
	cpu->regs.si.w = 0x0000;
	cpu->regs.di.w = 0x0000;

	cpu->regs.ax.w = 0x0000;
	cpu->regs.bx.w = 0x0000;
	cpu->regs.cx.w = 0x0000;
	cpu->regs.dx.w = 0x0000;

	cpu->flags.w = 0x0002;

	cpu->interrupt.irq     = 0;
	cpu->interrupt.irq_act = false;
	cpu->interrupt.nmi_act = false;

	i8086_reset(cpu);

}



void i8086_reset(CPU)
{

	cpu->regs.cs = 0xffff;
	cpu->regs.ip = 0x0000;

	cpu->regs.scs = cpu->regs.cs;
	cpu->regs.sip = cpu->regs.ip;

	cpu->insn.fetch_opcode   = true;
	cpu->insn.fetch_modrm    = false;
	cpu->insn.fetch_simm8a   = false;
	cpu->insn.fetch_uimm16a  = false;
	cpu->insn.fetch_simm8b   = false;
	cpu->insn.fetch_uimm16b  = false;
	cpu->insn.compute_eaimm0 = false;
	cpu->insn.compute_easeg  = false;
	cpu->insn.repeat_eq      = false;
	cpu->insn.repeat_ne      = false;
	cpu->insn.complete       = false;

	cpu->insn.opcode  = 0;
	cpu->insn.modrm   = 0;
	cpu->insn.segment = 255;

}



int i8086_intrq(CPU, uint nmi, uint irq)
{

	cpu->interrupt.irq     = irq;
	cpu->interrupt.irq_act = nmi == 0;
	cpu->interrupt.nmi_act = nmi != 0;

	return 0;

}



void i8086_interrupt(CPU, uint irq)
{

	decsp16(cpu); pushx16(cpu, cpu->flags.w);
	decsp16(cpu); pushx16(cpu, cpu->regs.cs);
	decsp16(cpu); pushx16(cpu, cpu->regs.ip);

	cpu->flags.i = false;
	cpu->flags.t = false;
	cpu->flags.a = false;

	cpu->regs.ip = bus_read16(&cpu->mem, 4 * (u8)irq + 0);
	cpu->regs.cs = bus_read16(&cpu->mem, 4 * (u8)irq + 2);

}



void i8086_tick(CPU)
{

	if (cpu->interrupt.nmi_act) {

		i8086_interrupt(cpu, I8086_VECTOR_NMI);

		cpu->interrupt.nmi_act = false;
		cpu->insn.fetch_opcode = true;

	} else if (cpu->flags.t && cpu->insn.fetch_opcode) {

		i8086_interrupt(cpu, I8086_VECTOR_SSTEP);

		cpu->insn.fetch_opcode = true;

	} else if (cpu->interrupt.irq_act) {

		i8086_interrupt(cpu, cpu->interrupt.irq);

		cpu->interrupt.irq_act = false;
		cpu->insn.fetch_opcode = true;

	}


	if (cpu->insn.fetch_opcode) { // Fetch and decode opcode

		const uint op  = bus_read8(&cpu->mem, cpu->regs.cs * 16 + cpu->regs.ip++);
		const uint opf = opflags[op];

		cpu->insn.opcode         = op;
		cpu->insn.modrm          = 0;
		cpu->insn.fetch_opcode   = false;
		cpu->insn.fetch_modrm    = opf & OP_MODRM;
		cpu->insn.fetch_simm8a   = opf & OP_I0B;
		cpu->insn.fetch_uimm16a  = opf & OP_I0W;
		cpu->insn.fetch_simm8b   = opf & OP_I1B;
		cpu->insn.fetch_uimm16b  = opf & OP_I1W;
		cpu->insn.compute_eaimm0 = opf & OP_EAIMM0;
		cpu->insn.compute_easeg  = opf & OP_EASEG;
		cpu->insn.compute_neaseg = opf & OP_NEASEG;
		cpu->insn.complete       = true;

		cpu->insn.addr = 0;
		cpu->insn.imm0 = 0;
		cpu->insn.imm1 = 0;

		cpu->insn.op_wide     = opf & OP_W16;
		cpu->insn.op_reverse  = opf & OP_REV;
		cpu->insn.op_rseg     = opf & OP_RSEG;
		cpu->insn.op_group    = opf & OP_GROUP;
		cpu->insn.op_override = opf & OP_OVRD;
		cpu->insn.op_memory   = false;

		cpu->insn.reg0b = NULL;
		cpu->insn.reg0w = NULL;
		cpu->insn.reg1b = NULL;
		cpu->insn.reg1w = NULL;

		if (opf & OP_R02)
			if (cpu->insn.op_wide) cpu->insn.reg0w = get_r16(cpu, op & 0x07);
			else                   cpu->insn.reg0b = get_r8( cpu, op & 0x07);

		if (opf & OP_GROUP)
			cpu->insn.opcode = 256 + (opf & 0xff) * 8;

	}


	if (cpu->insn.fetch_modrm) { // Fetch and decode MODRM

		cpu->insn.modrm       = bus_read8(&cpu->mem, cpu->regs.cs * 16 + cpu->regs.ip++);
		cpu->insn.fetch_modrm = false;

		const uint mod = cpu->insn.modrm & 0xc0;
		const uint reg = cpu->insn.modrm & 0x38;
		const uint rm  = cpu->insn.modrm & 0x07;

		bool addr_ss = false;

		if      (cpu->insn.op_group) cpu->insn.opcode += reg >> 3;
		else if (cpu->insn.op_rseg)  cpu->insn.reg0w = get_rs( cpu, reg >> 3);
		else if (cpu->insn.op_wide)  cpu->insn.reg0w = get_r16(cpu, reg >> 3);
		else                         cpu->insn.reg0b = get_r8( cpu, reg >> 3);

		if (mod != 0xc0) { // Memory mode

			cpu->insn.fetch_simm8a  = (mod == 0x40);
			cpu->insn.fetch_uimm16a = (mod == 0x80) || (mod == 0x00 && rm == 0x06);

			if (rm & 0x04) {

				cpu->insn.addr =
					(rm == 0x04)? cpu->regs.si.w:
					(rm == 0x05)? cpu->regs.di.w:
					(rm == 0x06)?
						((mod != 0x00)? cpu->regs.bp.w: 0):
						cpu->regs.bx.w;

				addr_ss = (mod != 0x00) && (rm == 0x06);

			} else {

				cpu->insn.addr  = (rm & 0x01)? cpu->regs.di.w: cpu->regs.si.w;
				cpu->insn.addr += (rm & 0x02)? cpu->regs.bp.w: cpu->regs.bx.w;

				addr_ss = (rm & 0x02) != 0;

			}

			if (cpu->insn.segment == 255)
				cpu->insn.segment = addr_ss? 2: 3;

			cpu->insn.compute_eaimm0 = cpu->insn.fetch_simm8a | cpu->insn.fetch_uimm16a;
			cpu->insn.compute_easeg  = true;
			cpu->insn.op_memory      = true;

		} else { // Register mode

			if (cpu->insn.op_wide) cpu->insn.reg1w = get_r16(cpu, rm);
			else                   cpu->insn.reg1b = get_r8( cpu, rm);

		}

	}


	if (cpu->insn.fetch_simm8a) { // Read a sign-extended 8bit value as first immediate operand

		cpu->insn.imm0         = (i8)bus_read8(&cpu->mem, cpu->regs.cs * 16 + cpu->regs.ip);
		cpu->insn.fetch_simm8a = false;

		cpu->regs.ip++;

	}


	if (cpu->insn.fetch_uimm16a) { // Read a 16bit value as first immediate operand

		cpu->insn.imm0          = bus_read16(&cpu->mem, cpu->regs.cs * 16 + cpu->regs.ip);
		cpu->insn.fetch_uimm16a = false;

		cpu->regs.ip += 2;

	}


	if (cpu->insn.fetch_simm8b) { // Read a sign-extended 8bit value as second immediate operand

		cpu->insn.imm1         = (i8)bus_read8(&cpu->mem, cpu->regs.cs * 16 + cpu->regs.ip);
		cpu->insn.fetch_simm8b = false;

		cpu->regs.ip++;

	}


	if (cpu->insn.fetch_uimm16b) { // Read a 16bit value as second immediate operand

		cpu->insn.imm1          = bus_read16(&cpu->mem, cpu->regs.cs * 16 + cpu->regs.ip);
		cpu->insn.fetch_uimm16b = false;

		cpu->regs.ip += 2;

	}


	if (cpu->insn.compute_eaimm0) {

		cpu->insn.addr           += cpu->insn.imm0;
		cpu->insn.compute_eaimm0  = false;

	}


	if (cpu->insn.compute_easeg) {

		cpu->insn.addr &= 0xffffu;

		if (!cpu->insn.compute_neaseg) {
			if   (cpu->insn.segment >= 0) cpu->insn.addr += 16 * *get_rs(cpu, cpu->insn.segment);
			else                          cpu->insn.addr += 16 * cpu->regs.ds;
		}

		cpu->insn.compute_easeg = false;

	}

	opcodes[cpu->insn.opcode](cpu);

	if (cpu->insn.complete) {

		cpu->insn.fetch_opcode = true;

		if (!cpu->insn.op_override) {

			cpu->regs.scs = cpu->regs.cs;
			cpu->regs.sip = cpu->regs.ip;

			cpu->insn.repeat_eq = false;
			cpu->insn.repeat_ne = false;
			cpu->insn.segment   = 255;

		}

	}

}



u8 *get_r8(CPU, uint r)
{

	switch (r & 7) {

		case 0: return &cpu->regs.ax.l;
		case 1: return &cpu->regs.cx.l;
		case 2: return &cpu->regs.dx.l;
		case 3: return &cpu->regs.bx.l;
		case 4: return &cpu->regs.ax.h;
		case 5: return &cpu->regs.cx.h;
		case 6: return &cpu->regs.dx.h;
		case 7: return &cpu->regs.bx.h;

	}

}



u16 *get_r16(CPU, uint r)
{

	switch (r & 7) {

		case 0: return &cpu->regs.ax.w;
		case 1: return &cpu->regs.cx.w;
		case 2: return &cpu->regs.dx.w;
		case 3: return &cpu->regs.bx.w;
		case 4: return &cpu->regs.sp.w;
		case 5: return &cpu->regs.bp.w;
		case 6: return &cpu->regs.si.w;
		case 7: return &cpu->regs.di.w;

	}

}



u16 *get_rs(CPU, uint r)
{

	switch (r & 3) {

		case 0: return &cpu->regs.es;
		case 1: return &cpu->regs.cs;
		case 2: return &cpu->regs.ss;
		case 3: return &cpu->regs.ds;

	}

}



void op_adcaib(CPU) { cpu->regs.ax.l = adc8( cpu, cpu->regs.ax.l, (u8)cpu->insn.imm0); }
void op_adcaiw(CPU) { cpu->regs.ax.w = adc16(cpu, cpu->regs.ax.w, (u16)cpu->insn.imm0); }


void op_adcrmib(CPU)
{

	aluu tmp;

	if (cpu->insn.op_memory) tmp = bus_read8(&cpu->mem, cpu->insn.addr);
	else                     tmp = *cpu->insn.reg1b;

	tmp = adc8(cpu, tmp, (u8)cpu->insn.imm1);

	if (cpu->insn.op_memory) bus_write8(&cpu->mem, cpu->insn.addr, tmp);
	else                     *cpu->insn.reg1b = tmp;

}



void op_adcrmiw(CPU)
{

	aluu tmp;

	if (cpu->insn.op_memory) tmp = bus_read16(&cpu->mem, cpu->insn.addr);
	else                     tmp = *cpu->insn.reg1w;

	tmp = adc16(cpu, tmp, (u16)cpu->insn.imm1);

	if (cpu->insn.op_memory) bus_write16(&cpu->mem, cpu->insn.addr, tmp);
	else                     *cpu->insn.reg1w = tmp;

}



void op_adcrmb(CPU)
{

	aluu tmp;

	if (cpu->insn.op_memory) tmp = bus_read8(&cpu->mem, cpu->insn.addr);
	else                     tmp = *cpu->insn.reg1b;

	if (cpu->insn.op_reverse) {

		tmp = adc8(cpu, tmp, *cpu->insn.reg0b);

		if (cpu->insn.op_memory) bus_write8(&cpu->mem, cpu->insn.addr, tmp);
		else                     *cpu->insn.reg1b = tmp;

	} else
		*cpu->insn.reg0b = adc8(cpu, *cpu->insn.reg0b, tmp);

}



void op_adcrmw(CPU)
{

	aluu tmp;

	if (cpu->insn.op_memory) tmp = bus_read16(&cpu->mem, cpu->insn.addr);
	else                     tmp = *cpu->insn.reg1w;

	if (cpu->insn.op_reverse) {

		tmp = adc16(cpu, tmp, *cpu->insn.reg0w);

		if (cpu->insn.op_memory) bus_write16(&cpu->mem, cpu->insn.addr, tmp);
		else                     *cpu->insn.reg1w = tmp;

	} else
		*cpu->insn.reg0w = adc16(cpu, *cpu->insn.reg0w, tmp);

}



void op_addaib(CPU) { cpu->regs.ax.l = add8( cpu, cpu->regs.ax.l, (u8)cpu->insn.imm0); }
void op_addaiw(CPU) { cpu->regs.ax.w = add16(cpu, cpu->regs.ax.w, (u16)cpu->insn.imm0); }



void op_addrmib(CPU)
{

	aluu tmp;

	if (cpu->insn.op_memory) tmp = bus_read8(&cpu->mem, cpu->insn.addr);
	else                     tmp = *cpu->insn.reg1b;

	tmp = add8(cpu, tmp, (u8)cpu->insn.imm1);

	if (cpu->insn.op_memory) bus_write8(&cpu->mem, cpu->insn.addr, tmp);
	else                     *cpu->insn.reg1b = tmp;

}



void op_addrmiw(CPU)
{

	aluu tmp;

	if (cpu->insn.op_memory) tmp = bus_read16(&cpu->mem, cpu->insn.addr);
	else                     tmp = *cpu->insn.reg1w;

	tmp = add16(cpu, tmp, (u16)cpu->insn.imm1);

	if (cpu->insn.op_memory) bus_write16(&cpu->mem, cpu->insn.addr, tmp);
	else                     *cpu->insn.reg1w = tmp;

}



void op_addrmb(CPU)
{

	aluu tmp;

	if (cpu->insn.op_memory) tmp = bus_read8(&cpu->mem, cpu->insn.addr);
	else                     tmp = *cpu->insn.reg1b;

	if (cpu->insn.op_reverse) {

		tmp = add8(cpu, tmp, *cpu->insn.reg0b);

		if (cpu->insn.op_memory) bus_write8(&cpu->mem, cpu->insn.addr, tmp);
		else                     *cpu->insn.reg1b = tmp;

	} else
		*cpu->insn.reg0b = add8(cpu, *cpu->insn.reg0b, tmp);

}



void op_addrmw(CPU)
{

	aluu tmp;

	if (cpu->insn.op_memory) tmp = bus_read16(&cpu->mem, cpu->insn.addr);
	else                     tmp = *cpu->insn.reg1w;

	if (cpu->insn.op_reverse) {

		tmp = add16(cpu, tmp, *cpu->insn.reg0w);

		if (cpu->insn.op_memory) bus_write16(&cpu->mem, cpu->insn.addr, tmp);
		else                     *cpu->insn.reg1w = tmp;

	} else
		*cpu->insn.reg0w = add16(cpu, *cpu->insn.reg0w, tmp);


}



void op_cmpaib(CPU) { sub8( cpu, cpu->regs.ax.l, (u8)cpu->insn.imm0); }
void op_cmpaiw(CPU) { sub16(cpu, cpu->regs.ax.w, (u16)cpu->insn.imm0); }



void op_cmprmib(CPU)
{

	aluu tmp;

	if (cpu->insn.op_memory) tmp = bus_read8(&cpu->mem, cpu->insn.addr);
	else                     tmp = *cpu->insn.reg1b;

	sub8(cpu, tmp, (u8)cpu->insn.imm1);

}



void op_cmprmiw(CPU)
{

	aluu tmp;

	if (cpu->insn.op_memory) tmp = bus_read16(&cpu->mem, cpu->insn.addr);
	else                     tmp = *cpu->insn.reg1w;

	sub16(cpu, tmp, (u16)cpu->insn.imm1);

}



void op_cmprmb(CPU)
{

	if (cpu->insn.op_reverse) {

		if (cpu->insn.op_memory) sub8(cpu, bus_read8(&cpu->mem, cpu->insn.addr), *cpu->insn.reg0b);
		else                     sub8(cpu, *cpu->insn.reg1b, *cpu->insn.reg0b);

	} else {

		if (cpu->insn.op_memory) sub8(cpu, *cpu->insn.reg0b, bus_read8(&cpu->mem, cpu->insn.addr));
		else                     sub8(cpu, *cpu->insn.reg0b, *cpu->insn.reg1b);

	}

}



void op_cmprmw(CPU)
{

	if (cpu->insn.op_reverse) {

		if (cpu->insn.op_memory) sub16(cpu, bus_read16(&cpu->mem, cpu->insn.addr), *cpu->insn.reg0w);
		else                     sub16( cpu, *cpu->insn.reg1w, *cpu->insn.reg0w);

	} else {

		if (cpu->insn.op_memory) sub16(cpu, *cpu->insn.reg0w, bus_read16(&cpu->mem, cpu->insn.addr));
		else                     sub16(cpu, *cpu->insn.reg0w, *cpu->insn.reg1w);

	}

}



void op_decrmb(CPU)
{
	if (cpu->insn.op_memory) bus_write8(&cpu->mem, cpu->insn.addr, dec8(cpu, bus_read8(&cpu->mem, cpu->insn.addr)));
	else                     *cpu->insn.reg1b = dec8(cpu, *cpu->insn.reg1b);
}



void op_decrmw(CPU)
{
	if (cpu->insn.op_memory) bus_write16(&cpu->mem, cpu->insn.addr, dec16(cpu, bus_read16(&cpu->mem, cpu->insn.addr)));
	else                     *cpu->insn.reg1w = dec16(cpu, *cpu->insn.reg1w);
}



void op_decrw(CPU) { *cpu->insn.reg0w = dec16(cpu, *cpu->insn.reg0w); }



void op_divrmb(CPU)
{

	aluu tmp;

	if (cpu->insn.op_memory) tmp = bus_read8(&cpu->mem, cpu->insn.addr);
	else                     tmp = *cpu->insn.reg1b;

	cpu->insn.imm0 = 0;

	if (tmp == 0)
		return i8086_interrupt(cpu, I8086_VECTOR_DIVERR);

	const aluu div = cpu->regs.ax.w / tmp;
	const aluu mod = cpu->regs.ax.w % tmp;

	if (div > 255)
		return i8086_interrupt(cpu, I8086_VECTOR_DIVERR);

	cpu->regs.ax.l = div;
	cpu->regs.ax.h = mod;

}



void op_divrmw(CPU)
{

	aluu tmp;

	if (cpu->insn.op_memory) tmp = bus_read16(&cpu->mem, cpu->insn.addr);
	else                     tmp = *cpu->insn.reg1w;

	cpu->insn.imm0 = 0;

	if (tmp == 0)
		return i8086_interrupt(cpu, I8086_VECTOR_DIVERR);

	const aluu num = cpu->regs.dx.w * 65536 + cpu->regs.ax.w;
	const aluu div = num / tmp;
	const aluu mod = num % tmp;

	if (div > 65535)
		return i8086_interrupt(cpu, I8086_VECTOR_DIVERR);

	cpu->regs.ax.w = div;
	cpu->regs.dx.w = mod;

}



void op_idivrmb(CPU)
{

	alui tmp;

	if (cpu->insn.op_memory) tmp = (i8)bus_read8(&cpu->mem, cpu->insn.addr);
	else                     tmp = (i8)*cpu->insn.reg1b;

	cpu->insn.imm0 = 0;

	if (tmp == 0)
		return i8086_interrupt(cpu, I8086_VECTOR_DIVERR);

	const alui num = (i16)cpu->regs.ax.w;
	const alui div = num / tmp;
	const alui mod = num % tmp;

	if (div < -128 || div > 127)
		return i8086_interrupt(cpu, I8086_VECTOR_DIVERR);

	cpu->regs.ax.l = div;
	cpu->regs.ax.h = mod;

}



void op_idivrmw(CPU)
{

	alui tmp;

	if (cpu->insn.op_memory) tmp = (i16)bus_read16(&cpu->mem, cpu->insn.addr);
	else                     tmp = (i16)*cpu->insn.reg1w;

	cpu->insn.imm0 = 0;

	if (tmp == 0)
		return i8086_interrupt(cpu, I8086_VECTOR_DIVERR);

	const alui num = (i32)(cpu->regs.dx.w * 65556 + cpu->regs.ax.w);
	const alui div = num / tmp;
	const alui mod = num % tmp;

	if (div < -32768 || div > 32767)
		return i8086_interrupt(cpu, I8086_VECTOR_DIVERR);

	cpu->regs.ax.w = div;
	cpu->regs.dx.w = mod;

}



void op_imulrmb(CPU)
{

	aluu tmp;

	if (cpu->insn.op_memory) tmp = (i8)bus_read8(&cpu->mem, cpu->insn.addr);
	else                     tmp = (i8)*cpu->insn.reg1b;

	tmp *= (i8)cpu->regs.ax.l;

	cpu->regs.ax.w = tmp;

	cpu->flags.c = (i8)cpu->regs.ax.l != tmp;
	cpu->flags.v = cpu->flags.c;

}



void op_imulrmw(CPU)
{

	aluu tmp;

	if (cpu->insn.op_memory) tmp = (i16)bus_read16(&cpu->mem, cpu->insn.addr);
	else                     tmp = (i16)*cpu->insn.reg1w;

	tmp *= (i16)cpu->regs.ax.w;

	cpu->regs.ax.w = (tmp >> 0)  & 0xffff;
	cpu->regs.dx.w = (tmp >> 16) & 0xffff;

	cpu->flags.c = (i16)cpu->regs.ax.w != tmp;
	cpu->flags.v = cpu->flags.c;

}



void op_incrmb(CPU)
{
	if (cpu->insn.op_memory) bus_write8(&cpu->mem, cpu->insn.addr, inc8(cpu, bus_read8(&cpu->mem, cpu->insn.addr)));
	else                     *cpu->insn.reg1b = inc8(cpu, *cpu->insn.reg1b);
}



void op_incrmw(CPU)
{
	if (cpu->insn.op_memory) bus_write16(&cpu->mem, cpu->insn.addr, inc16(cpu, bus_read16(&cpu->mem, cpu->insn.addr)));
	else                     *cpu->insn.reg1w = inc16(cpu, *cpu->insn.reg1w);
}



void op_incrw(CPU) { *cpu->insn.reg0w = inc16(cpu, *cpu->insn.reg0w); }



void op_mulrmb(CPU)
{

	aluu tmp;

	if (cpu->insn.op_memory) tmp = bus_read8(&cpu->mem, cpu->insn.addr);
	else                     tmp = *cpu->insn.reg1b;

	cpu->regs.ax.w = cpu->regs.ax.l * tmp;

	cpu->flags.c = (cpu->regs.ax.h != 0);
	cpu->flags.v = cpu->flags.c;

}



void op_mulrmw(CPU)
{

	aluu tmp;

	if (cpu->insn.op_memory) tmp = bus_read16(&cpu->mem, cpu->insn.addr);
	else                     tmp = *cpu->insn.reg1w;

	tmp *= cpu->regs.ax.w;

	cpu->regs.ax.w = (tmp >> 0)  & 0xffff;
	cpu->regs.dx.w = (tmp >> 16) & 0xffff;

	cpu->flags.c = (cpu->regs.dx.w != 0);
	cpu->flags.v = cpu->flags.c;

}



void op_negrmb(CPU)
{
	aluu tmp;

	if (cpu->insn.op_memory) tmp = bus_read8(&cpu->mem, cpu->insn.addr);
	else                     tmp = *cpu->insn.reg1b;

	tmp = sub8(cpu, 0, tmp);

	if (cpu->insn.op_memory) bus_write8(&cpu->mem, cpu->insn.addr, tmp);
	else                     *cpu->insn.reg1b = tmp;

}



void op_negrmw(CPU)
{
	aluu tmp;

	if (cpu->insn.op_memory) tmp = bus_read16(&cpu->mem, cpu->insn.addr);
	else                     tmp = *cpu->insn.reg1w;

	tmp = sub16(cpu, 0, tmp);

	if (cpu->insn.op_memory) bus_write16(&cpu->mem, cpu->insn.addr, tmp);
	else                     *cpu->insn.reg1w = tmp;

}



void op_sbbaib(CPU) { cpu->regs.ax.l = sbb8( cpu, cpu->regs.ax.l, (u8)cpu->insn.imm0); }
void op_sbbaiw(CPU) { cpu->regs.ax.w = sbb16(cpu, cpu->regs.ax.w, (u16)cpu->insn.imm0); }



void op_sbbrmib(CPU)
{

	aluu tmp;

	if (cpu->insn.op_memory) tmp = bus_read8(&cpu->mem, cpu->insn.addr);
	else                     tmp = *cpu->insn.reg1b;

	tmp = sbb8(cpu, tmp, (u8)cpu->insn.imm1);

	if (cpu->insn.op_memory) bus_write8(&cpu->mem, cpu->insn.addr, tmp);
	else                     *cpu->insn.reg1b = tmp;

}




void op_sbbrmiw(CPU)
{

	aluu tmp;

	if (cpu->insn.op_memory) tmp = bus_read16(&cpu->mem, cpu->insn.addr);
	else                     tmp = *cpu->insn.reg1w;

	tmp = sbb16(cpu, tmp, (u16)cpu->insn.imm1);

	if (cpu->insn.op_memory) bus_write16(&cpu->mem, cpu->insn.addr, tmp);
	else                     *cpu->insn.reg1w = tmp;

}



void op_sbbrmb(CPU)
{
	aluu tmp;

	if (cpu->insn.op_memory) tmp = bus_read8(&cpu->mem, cpu->insn.addr);
	else                     tmp = *cpu->insn.reg1b;

	if (cpu->insn.op_reverse) {

		tmp = sbb8(cpu, tmp, *cpu->insn.reg0b);

		if (cpu->insn.op_memory) bus_write8(&cpu->mem, cpu->insn.addr, tmp);
		else                     *cpu->insn.reg1b = tmp;

	} else
		*cpu->insn.reg0b = sbb8(cpu, *cpu->insn.reg0b, tmp);

}



void op_sbbrmw(CPU)
{

	aluu tmp;

	if (cpu->insn.op_memory) tmp = bus_read16(&cpu->mem, cpu->insn.addr);
	else                     tmp = *cpu->insn.reg1w;

	if (cpu->insn.op_reverse) {

		tmp = sbb16(cpu, tmp, *cpu->insn.reg0w);

		if (cpu->insn.op_memory) bus_write16(&cpu->mem, cpu->insn.addr, tmp);
		else                     *cpu->insn.reg1w = tmp;

	} else
		*cpu->insn.reg0w = sbb16(cpu, *cpu->insn.reg0w, tmp);

}



void op_subaib(CPU) { cpu->regs.ax.l = sub8( cpu, cpu->regs.ax.l, (u8)cpu->insn.imm0); }
void op_subaiw(CPU) { cpu->regs.ax.w = sub16(cpu, cpu->regs.ax.w, (u16)cpu->insn.imm0); }



void op_subrib(CPU)
{

	aluu tmp;

	if (cpu->insn.op_memory) tmp = bus_read8(&cpu->mem, cpu->insn.addr);
	else                     tmp = *cpu->insn.reg1b;

	tmp = sub8(cpu, tmp, (u8)cpu->insn.imm1);

	if (cpu->insn.op_memory) bus_write8(&cpu->mem, cpu->insn.addr, tmp);
	else                     *cpu->insn.reg1b = tmp;

}




void op_subriw(CPU)
{

	aluu tmp;

	if (cpu->insn.op_memory) tmp = bus_read16(&cpu->mem, cpu->insn.addr);
	else                     tmp = *cpu->insn.reg1w;

	tmp = sub16(cpu, tmp, (u16)cpu->insn.imm1);

	if (cpu->insn.op_memory) bus_write16(&cpu->mem, cpu->insn.addr, tmp);
	else                     *cpu->insn.reg1w = tmp;

}



void op_subrmb(CPU)
{
	aluu tmp;

	if (cpu->insn.op_memory) tmp = bus_read8(&cpu->mem, cpu->insn.addr);
	else                     tmp = *cpu->insn.reg1b;

	if (cpu->insn.op_reverse) {

		tmp = sub8(cpu, tmp, *cpu->insn.reg0b);

		if (cpu->insn.op_memory) bus_write8(&cpu->mem, cpu->insn.addr, tmp);
		else                     *cpu->insn.reg1b = tmp;

	} else
		*cpu->insn.reg0b = sub8(cpu, *cpu->insn.reg0b, tmp);

}



void op_subrmw(CPU)
{
	aluu tmp;

	if (cpu->insn.op_memory) tmp = bus_read16(&cpu->mem, cpu->insn.addr);
	else                     tmp = *cpu->insn.reg1w;

	if (cpu->insn.op_reverse) {

		tmp = sub16(cpu, tmp, *cpu->insn.reg0w);

		if (cpu->insn.op_memory) bus_write16(&cpu->mem, cpu->insn.addr, tmp);
		else                     *cpu->insn.reg1w = tmp;

	} else
		*cpu->insn.reg0w = sub16(cpu, *cpu->insn.reg0w, tmp);

}



void op_andaib(CPU) { cpu->regs.ax.l = and8( cpu, cpu->regs.ax.l, cpu->insn.imm0); }
void op_andaiw(CPU) { cpu->regs.ax.w = and16(cpu, cpu->regs.ax.w, cpu->insn.imm0); }



void op_andrib(CPU)
{

	aluu tmp;

	if (cpu->insn.op_memory) tmp = bus_read8(&cpu->mem, cpu->insn.addr);
	else                     tmp = *cpu->insn.reg1b;

	tmp = and8(cpu, tmp, (u8)cpu->insn.imm1);

	if (cpu->insn.op_memory) bus_write8(&cpu->mem, cpu->insn.addr, tmp);
	else                     *cpu->insn.reg1b = tmp;

}



void op_andriw(CPU)
{

	aluu tmp;

	if (cpu->insn.op_memory) tmp = bus_read16(&cpu->mem, cpu->insn.addr);
	else                     tmp = *cpu->insn.reg1w;

	tmp = and16(cpu, tmp, (u16)cpu->insn.imm1);

	if (cpu->insn.op_memory) bus_write16(&cpu->mem, cpu->insn.addr, tmp);
	else                     *cpu->insn.reg1w = tmp;

}



void op_andrmb(CPU)
{

	if (cpu->insn.op_reverse) {

		if (cpu->insn.op_memory) bus_write8(&cpu->mem, cpu->insn.addr, and8(cpu, bus_read8(&cpu->mem, cpu->insn.addr), *cpu->insn.reg0b));
		else                     *cpu->insn.reg1b = and8(cpu, *cpu->insn.reg1b, *cpu->insn.reg0b);

	} else {

		if (cpu->insn.op_memory) *cpu->insn.reg0b = and8(cpu, *cpu->insn.reg0b, bus_read8(&cpu->mem, cpu->insn.addr));
		else                     *cpu->insn.reg0b = and8(cpu, *cpu->insn.reg0b, *cpu->insn.reg1b);

	}

}



void op_andrmw(CPU)
{

	if (cpu->insn.op_reverse) {

		if (cpu->insn.op_memory) bus_write16(&cpu->mem, cpu->insn.addr, and16(cpu, bus_read16(&cpu->mem, cpu->insn.addr), *cpu->insn.reg0w));
		else                     *cpu->insn.reg1w = and16(cpu, *cpu->insn.reg1w, *cpu->insn.reg0w);

	} else {

		if (cpu->insn.op_memory) *cpu->insn.reg0w = and16(cpu, *cpu->insn.reg0w, bus_read16(&cpu->mem, cpu->insn.addr));
		else                     *cpu->insn.reg0w = and16(cpu, *cpu->insn.reg0w, *cpu->insn.reg1w);

	}

}



void op_ioraib(CPU) { cpu->regs.ax.l = ior8( cpu, cpu->regs.ax.l, (u8)cpu->insn.imm0); }
void op_ioraiw(CPU) { cpu->regs.ax.w = ior16(cpu, cpu->regs.ax.w, (u16)cpu->insn.imm0); }



void op_iorrib(CPU)
{

	aluu tmp;

	if (cpu->insn.op_memory) tmp = bus_read8(&cpu->mem, cpu->insn.addr);
	else                     tmp = *cpu->insn.reg1b;

	tmp = ior8(cpu, tmp, (u8)cpu->insn.imm1);

	if (cpu->insn.op_memory) bus_write8(&cpu->mem, cpu->insn.addr, tmp);
	else                     *cpu->insn.reg1b = tmp;

}



void op_iorriw(CPU)
{

	aluu tmp;

	if (cpu->insn.op_memory) tmp = bus_read16(&cpu->mem, cpu->insn.addr);
	else                     tmp = *cpu->insn.reg1w;

	tmp = ior16(cpu, tmp, (u16)cpu->insn.imm1);

	if (cpu->insn.op_memory) bus_write16(&cpu->mem, cpu->insn.addr, tmp);
	else                     *cpu->insn.reg1w = tmp;

}



void op_iorrmb(CPU)
{

	if (cpu->insn.op_reverse) {

		if (cpu->insn.op_memory) bus_write8(&cpu->mem, cpu->insn.addr, ior8(cpu, bus_read8(&cpu->mem, cpu->insn.addr), *cpu->insn.reg0b));
		else                     *cpu->insn.reg1b = ior8(cpu, *cpu->insn.reg1b, *cpu->insn.reg0b);

	} else {

		if (cpu->insn.op_memory) *cpu->insn.reg0b = ior8(cpu, *cpu->insn.reg0b, bus_read8(&cpu->mem, cpu->insn.addr));
		else                     *cpu->insn.reg0b = ior8(cpu, *cpu->insn.reg0b, *cpu->insn.reg1b);

	}

}



void op_iorrmw(CPU)
{

	if (cpu->insn.op_reverse) {

		if (cpu->insn.op_memory) bus_write16(&cpu->mem, cpu->insn.addr, ior16(cpu, bus_read16(&cpu->mem, cpu->insn.addr), *cpu->insn.reg0w));
		else                     *cpu->insn.reg1w = ior16(cpu, *cpu->insn.reg1w, *cpu->insn.reg0w);

	} else {

		if (cpu->insn.op_memory) *cpu->insn.reg0w = ior16(cpu, *cpu->insn.reg0w, bus_read16(&cpu->mem, cpu->insn.addr));
		else                     *cpu->insn.reg0w = ior16(cpu, *cpu->insn.reg0w, *cpu->insn.reg1w);

	}

}



void op_notrmb(CPU)
{
	if (cpu->insn.op_memory) bus_write8(&cpu->mem, cpu->insn.addr, ~bus_read8(&cpu->mem, cpu->insn.addr));
	else                     *cpu->insn.reg1b = ~*cpu->insn.reg1b;
}



void op_notrmw(CPU)
{
	if (cpu->insn.op_memory) bus_write16(&cpu->mem, cpu->insn.addr, ~bus_read16(&cpu->mem, cpu->insn.addr));
	else                     *cpu->insn.reg1w = ~*cpu->insn.reg1w;
}



void op_tstaib(CPU) { and8( cpu, cpu->regs.ax.l, (u8)cpu->insn.imm0); }
void op_tstaiw(CPU) { and16(cpu, cpu->regs.ax.w, (u16)cpu->insn.imm0); }



void op_tstrib(CPU)
{

	aluu tmp, arg0;

	// Manually fetch first immediate operand
	arg0 = bus_read8(&cpu->mem, cpu->regs.cs * 16 + cpu->regs.ip);
	cpu->regs.ip++;

	if (cpu->insn.op_memory) tmp = bus_read8(&cpu->mem, cpu->insn.addr);
	else                     tmp = *cpu->insn.reg1b;

	and8(cpu, tmp, arg0);

}



void op_tstriw(CPU)
{

	aluu tmp, arg0;

	// Manually fetch first immediate operand
	arg0          = bus_read16(&cpu->mem, cpu->regs.cs * 16 + cpu->regs.ip);
	cpu->regs.ip += 2;

	if (cpu->insn.op_memory) tmp = bus_read16(&cpu->mem, cpu->insn.addr);
	else                     tmp = *cpu->insn.reg1w;

	and16(cpu, tmp, arg0);

}



void op_tstrmb(CPU)
{
	if (cpu->insn.op_memory) and8(cpu, bus_read8(&cpu->mem, cpu->insn.addr), *cpu->insn.reg0b);
	else                     and8( cpu, *cpu->insn.reg1b, *cpu->insn.reg0b);
}



void op_tstrmw(CPU)
{
	if (cpu->insn.op_memory) and16(cpu, bus_read16(&cpu->mem, cpu->insn.addr), *cpu->insn.reg0w);
	else                     and16( cpu, *cpu->insn.reg1w, *cpu->insn.reg0w);
}



void op_xoraib(CPU) { cpu->regs.ax.l = xor8( cpu, cpu->regs.ax.l, (u8)cpu->insn.imm0); }
void op_xoraiw(CPU) { cpu->regs.ax.w = xor16(cpu, cpu->regs.ax.w, (u16)cpu->insn.imm0); }



void op_xorrib(CPU)
{

	aluu tmp;

	if (cpu->insn.op_memory) tmp = bus_read8(&cpu->mem, cpu->insn.addr);
	else                     tmp = *cpu->insn.reg1b;

	tmp = xor8(cpu, tmp, (u8)cpu->insn.imm1);

	if (cpu->insn.op_memory) bus_write8(&cpu->mem, cpu->insn.addr, tmp);
	else                     *cpu->insn.reg1b = tmp;

}



void op_xorriw(CPU)
{

	aluu tmp;

	if (cpu->insn.op_memory) tmp = bus_read16(&cpu->mem, cpu->insn.addr);
	else                     tmp = *cpu->insn.reg1w;

	tmp = xor16(cpu, tmp, (u16)cpu->insn.imm1);

	if (cpu->insn.op_memory) bus_write16(&cpu->mem, cpu->insn.addr, tmp);
	else                     *cpu->insn.reg1w = tmp;

}



void op_xorrmb(CPU)
{

	if (cpu->insn.op_reverse) {

		if (cpu->insn.op_memory) bus_write8(&cpu->mem, cpu->insn.addr, xor8(cpu, bus_read8(&cpu->mem, cpu->insn.addr), *cpu->insn.reg0b));
		else                     *cpu->insn.reg1b = xor8(cpu, *cpu->insn.reg1b, *cpu->insn.reg0b);

	} else {

		if (cpu->insn.op_memory) *cpu->insn.reg0b = xor8(cpu, *cpu->insn.reg0b, bus_read8(&cpu->mem, cpu->insn.addr));
		else                     *cpu->insn.reg0b = xor8(cpu, *cpu->insn.reg0b, *cpu->insn.reg1b);

	}

}



void op_xorrmw(CPU)
{

	if (cpu->insn.op_reverse) {

		if (cpu->insn.op_memory) bus_write16(&cpu->mem, cpu->insn.addr, xor16(cpu, bus_read16(&cpu->mem, cpu->insn.addr), *cpu->insn.reg0w));
		else                     *cpu->insn.reg1w = xor16(cpu, *cpu->insn.reg1w, *cpu->insn.reg0w);

	} else {

		if (cpu->insn.op_memory) *cpu->insn.reg0w = xor16(cpu, *cpu->insn.reg0w, bus_read16(&cpu->mem, cpu->insn.addr));
		else                     *cpu->insn.reg0w = xor16(cpu, *cpu->insn.reg0w, *cpu->insn.reg1w);

	}

}



void op_rclb1(CPU) {
	if (cpu->insn.op_memory) bus_write8(&cpu->mem, cpu->insn.addr, rcl8(cpu, bus_read8(&cpu->mem, cpu->insn.addr), 1));
	else                     *cpu->insn.reg1b = rcl8(cpu, (u8)*cpu->insn.reg1b, 1);
}

void op_rclbr(CPU) {
	const aluu c = cpu->regs.cx.l % 9;
	if (c == 0) return;
	if (cpu->insn.op_memory) bus_write8(&cpu->mem, cpu->insn.addr, rcl8(cpu, bus_read8(&cpu->mem, cpu->insn.addr), c));
	else                     *cpu->insn.reg1b = rcl8(cpu, *cpu->insn.reg1b, c);
}

void op_rclw1(CPU) {
	if (cpu->insn.op_memory) bus_write16(&cpu->mem, cpu->insn.addr, rcl16(cpu, bus_read16(&cpu->mem, cpu->insn.addr), 1));
	else                     *cpu->insn.reg1w = rcl16(cpu, *cpu->insn.reg1w, 1);
}

void op_rclwr(CPU) {
	const aluu c = cpu->regs.cx.l % 17;
	if (c == 0) return;
	if (cpu->insn.op_memory) bus_write16(&cpu->mem, cpu->insn.addr, rcl16(cpu, bus_read16(&cpu->mem, cpu->insn.addr), c));
	else                     *cpu->insn.reg1w = rcl16(cpu, *cpu->insn.reg1w, c);
}

void op_rcrb1(CPU) {
	if (cpu->insn.op_memory) bus_write8(&cpu->mem, cpu->insn.addr, rcr8(cpu, bus_read8(&cpu->mem, cpu->insn.addr), 1));
	else                     *cpu->insn.reg1b = rcr8(cpu, *cpu->insn.reg1b, 1);
}

void op_rcrbr(CPU) {
	const aluu c = cpu->regs.cx.l % 9;
	if (c == 0) return;
	if (cpu->insn.op_memory) bus_write8(&cpu->mem, cpu->insn.addr, rcr8(cpu, bus_read8(&cpu->mem, cpu->insn.addr), c));
	else                     *cpu->insn.reg1b = rcr8(cpu, *cpu->insn.reg1b, c);
}

void op_rcrw1(CPU) {
	if (cpu->insn.op_memory) bus_write16(&cpu->mem, cpu->insn.addr, rcr16(cpu, bus_read16(&cpu->mem, cpu->insn.addr), 1));
	else                     *cpu->insn.reg1w = rcr16(cpu, *cpu->insn.reg1w, 1);
}

void op_rcrwr(CPU) {
	const aluu c = cpu->regs.cx.l % 17;
	if (c == 0) return;
	if (cpu->insn.op_memory) bus_write16(&cpu->mem, cpu->insn.addr, rcr16(cpu, bus_read16(&cpu->mem, cpu->insn.addr), c));
	else                     *cpu->insn.reg1w = rcr16(cpu, *cpu->insn.reg1w, c);
}

void op_rolb1(CPU) {
	if (cpu->insn.op_memory) bus_write8(&cpu->mem, cpu->insn.addr, rol8(cpu, bus_read8(&cpu->mem, cpu->insn.addr), 1));
	else                     *cpu->insn.reg1b = rol8(cpu, *cpu->insn.reg1b, 1);
}

void op_rolbr(CPU) {
	const aluu c = cpu->regs.cx.l;
	if (c == 0) return;
	if (cpu->insn.op_memory) bus_write8(&cpu->mem, cpu->insn.addr, rol8(cpu, bus_read8(&cpu->mem, cpu->insn.addr), c));
	else                     *cpu->insn.reg1b = rol8(cpu, *cpu->insn.reg1b, c);
}

void op_rolw1(CPU) {
	if (cpu->insn.op_memory) bus_write16(&cpu->mem, cpu->insn.addr, rol16(cpu, bus_read16(&cpu->mem, cpu->insn.addr), 1));
	else                     *cpu->insn.reg1w = rol16(cpu, *cpu->insn.reg1w, 1);
}

void op_rolwr(CPU) {
	const aluu c = cpu->regs.cx.l;
	if (c == 0) return;
	if (cpu->insn.op_memory) bus_write16(&cpu->mem, cpu->insn.addr, rol16(cpu, bus_read16(&cpu->mem, cpu->insn.addr), c));
	else                     *cpu->insn.reg1w = rol16(cpu, *cpu->insn.reg1w, c);
}

void op_rorb1(CPU) {
	if (cpu->insn.op_memory) bus_write8(&cpu->mem, cpu->insn.addr, ror8(cpu, bus_read8(&cpu->mem, cpu->insn.addr), 1));
	else                     *cpu->insn.reg1b = ror8(cpu, *cpu->insn.reg1b, 1);
}

void op_rorbr(CPU) {
	const aluu c = cpu->regs.cx.l;
	if (c == 0) return;
	if (cpu->insn.op_memory) bus_write8(&cpu->mem, cpu->insn.addr, ror8(cpu, bus_read8(&cpu->mem, cpu->insn.addr), c));
	else                     *cpu->insn.reg1b = ror8(cpu, *cpu->insn.reg1b, c);
}

void op_rorw1(CPU) {
	if (cpu->insn.op_memory) bus_write16(&cpu->mem, cpu->insn.addr, ror16(cpu, bus_read16(&cpu->mem, cpu->insn.addr), 1));
	else                     *cpu->insn.reg1w = ror16(cpu, *cpu->insn.reg1w, 1);
}

void op_rorwr(CPU) {
	const aluu c = cpu->regs.cx.l;
	if (c == 0) return;
	if (cpu->insn.op_memory) bus_write16(&cpu->mem, cpu->insn.addr, ror16(cpu, bus_read16(&cpu->mem, cpu->insn.addr), c));
	else                     *cpu->insn.reg1w = ror16(cpu, *cpu->insn.reg1w, c);
}

void op_salb1(CPU) {
	if (cpu->insn.op_memory) bus_write8(&cpu->mem, cpu->insn.addr, sal8(cpu, bus_read8(&cpu->mem, cpu->insn.addr), 1));
	else                     *cpu->insn.reg1b = sal8(cpu, *cpu->insn.reg1b, 1);
}

void op_salbr(CPU) {
	const aluu c = cpu->regs.cx.l;
	if (c == 0) return;
	if (cpu->insn.op_memory) bus_write8(&cpu->mem, cpu->insn.addr, sal8(cpu, bus_read8(&cpu->mem, cpu->insn.addr), c));
	else                     *cpu->insn.reg1b = sal8(cpu, *cpu->insn.reg1b, c);
}

void op_salw1(CPU) {
	if (cpu->insn.op_memory) bus_write16(&cpu->mem, cpu->insn.addr, sal16(cpu, bus_read16(&cpu->mem, cpu->insn.addr), 1));
	else                     *cpu->insn.reg1w = sal16(cpu, *cpu->insn.reg1w, 1);
}

void op_salwr(CPU) {
	const aluu c = cpu->regs.cx.l;
	if (c == 0) return;
	if (cpu->insn.op_memory) bus_write16(&cpu->mem, cpu->insn.addr, sal16(cpu, bus_read16(&cpu->mem, cpu->insn.addr), c));
	else                     *cpu->insn.reg1w = sal16(cpu, *cpu->insn.reg1w, c);
}

void op_sarb1(CPU) {
	if (cpu->insn.op_memory) bus_write8(&cpu->mem, cpu->insn.addr, sar8(cpu, bus_read8(&cpu->mem, cpu->insn.addr), 1));
	else                     *cpu->insn.reg1b = sar8(cpu, *cpu->insn.reg1b, 1);
}

void op_sarbr(CPU) {
	const aluu c = cpu->regs.cx.l;
	if (c == 0) return;
	if (cpu->insn.op_memory) bus_write8(&cpu->mem, cpu->insn.addr, sar8(cpu, bus_read8(&cpu->mem, cpu->insn.addr), c));
	else                     *cpu->insn.reg1b = sar8(cpu, *cpu->insn.reg1b, c);
}

void op_sarw1(CPU) {
	if (cpu->insn.op_memory) bus_write16(&cpu->mem, cpu->insn.addr, sar16(cpu, bus_read16(&cpu->mem, cpu->insn.addr), 1));
	else                     *cpu->insn.reg1w = sar16(cpu, *cpu->insn.reg1w, 1);
}

void op_sarwr(CPU) {
	const aluu c = cpu->regs.cx.l;
	if (c == 0) return;
	if (cpu->insn.op_memory) bus_write16(&cpu->mem, cpu->insn.addr, sar16(cpu, bus_read16(&cpu->mem, cpu->insn.addr), c));
	else                     *cpu->insn.reg1w = sar16(cpu, *cpu->insn.reg1w, c);
}

void op_shlb1(CPU) {
	if (cpu->insn.op_memory) bus_write8(&cpu->mem, cpu->insn.addr, shl8(cpu, bus_read8(&cpu->mem, cpu->insn.addr), 1));
	else                     *cpu->insn.reg1b = shl8(cpu, *cpu->insn.reg1b, 1);
}

void op_shlbr(CPU) {
	const aluu c = cpu->regs.cx.l;
	if (c == 0) return;
	if (cpu->insn.op_memory) bus_write8(&cpu->mem, cpu->insn.addr, shl8(cpu, bus_read8(&cpu->mem, cpu->insn.addr), c));
	else                     *cpu->insn.reg1b = shl8(cpu, *cpu->insn.reg1b, c);
}

void op_shlw1(CPU) {
	if (cpu->insn.op_memory) bus_write16(&cpu->mem, cpu->insn.addr, shl16(cpu, bus_read16(&cpu->mem, cpu->insn.addr), 1));
	else                     *cpu->insn.reg1w = shl16(cpu, *cpu->insn.reg1w, 1);
}

void op_shlwr(CPU) {
	const aluu c = cpu->regs.cx.l;
	if (c == 0) return;
	if (cpu->insn.op_memory) bus_write16(&cpu->mem, cpu->insn.addr, shl16(cpu, bus_read16(&cpu->mem, cpu->insn.addr), c));
	else                     *cpu->insn.reg1w = shl16(cpu, *cpu->insn.reg1w, c);
}

void op_shrb1(CPU) {
	if (cpu->insn.op_memory) bus_write8(&cpu->mem, cpu->insn.addr, shr8(cpu, bus_read8(&cpu->mem, cpu->insn.addr), 1));
	else                     *cpu->insn.reg1b = shr8(cpu, *cpu->insn.reg1b, 1);
}

void op_shrbr(CPU) {
	const aluu c = cpu->regs.cx.l;
	if (c == 0) return;
	if (cpu->insn.op_memory) bus_write8(&cpu->mem, cpu->insn.addr, shr8(cpu, bus_read8(&cpu->mem, cpu->insn.addr), c));
	else                     *cpu->insn.reg1b = shr8(cpu, *cpu->insn.reg1b, c);
}

void op_shrw1(CPU) {
	if (cpu->insn.op_memory) bus_write16(&cpu->mem, cpu->insn.addr, shr16(cpu, bus_read16(&cpu->mem, cpu->insn.addr), 1));
	else                     *cpu->insn.reg1w = shr16(cpu, *cpu->insn.reg1w, 1);
}

void op_shrwr(CPU) {
	const aluu c = cpu->regs.cx.l;
	if (c == 0) return;
	if (cpu->insn.op_memory) bus_write16(&cpu->mem, cpu->insn.addr, shr16(cpu, bus_read16(&cpu->mem, cpu->insn.addr), c));
	else                     *cpu->insn.reg1w = shr16(cpu, *cpu->insn.reg1w, c);
}


void op_aaa(CPU) { op_undef(cpu); }
void op_aad(CPU) { op_undef(cpu); }
void op_aas(CPU) { op_undef(cpu); }
void op_aam(CPU) { if (cpu->insn.imm0) { aluu t = cpu->regs.ax.l; cpu->regs.ax.l = t % cpu->insn.imm0; cpu->regs.ax.h = t / cpu->insn.imm0; } }
void op_cbw(CPU) { cpu->regs.ax.w = (i8)cpu->regs.ax.l; }
void op_cwd(CPU) { cpu->regs.dx.w = sign(cpu->regs.ax.w, 0x8000)? 0xffff: 0x0000; }



void op_daa(CPU)
{

	aluu al = cpu->regs.ax.l;
	bool c  = cpu->flags.c;

	cpu->flags.c = false;

	if (((cpu->regs.ax.l & 0x0f) > 9) || cpu->flags.a) {

		cpu->regs.ax.l += 6;

		cpu->flags.c = c;// || (Carry from AL := AL + 6);
		cpu->flags.a = true;

	} else
		cpu->flags.a = false;


	if ((al > 0x99) || c) {

		cpu->regs.ax.l += 0x60;
		cpu->flags.c = true;

	} else
		cpu->flags.c = false;

}



void op_das(CPU) { op_undef(cpu); }



void op_lahf(CPU) { cpu->regs.ax.h = (u8)cpu->flags.w | 0x02; }



void op_ldsr(CPU)
{

	if (!cpu->insn.op_memory)
		return op_undef(cpu);

	*cpu->insn.reg0w = bus_read16(&cpu->mem, cpu->insn.addr + 0);
	cpu->regs.ds     = bus_read16(&cpu->mem, cpu->insn.addr + 2);

}



void op_lesr(CPU)
{

	if (!cpu->insn.op_memory)
		return op_undef(cpu);

	*cpu->insn.reg0w = bus_read16(&cpu->mem, cpu->insn.addr + 0);
	cpu->regs.es     = bus_read16(&cpu->mem, cpu->insn.addr + 2);

}



void op_lear(CPU)
{

	if (!cpu->insn.op_memory)
		return op_undef(cpu);

	*cpu->insn.reg0w = cpu->insn.addr;

}


void op_movamb(CPU)
{
	if (cpu->insn.op_reverse) bus_write8(&cpu->mem, cpu->insn.addr, cpu->regs.ax.l);
	else                      cpu->regs.ax.l = bus_read8(&cpu->mem, cpu->insn.addr);
}



void op_movamw(CPU)
{
	if (cpu->insn.op_reverse) bus_write16(&cpu->mem, cpu->insn.addr, cpu->regs.ax.w);
	else                      cpu->regs.ax.w = bus_read16(&cpu->mem, cpu->insn.addr);
}



void op_movrib(CPU) { *cpu->insn.reg0b = cpu->insn.imm0; }
void op_movriw(CPU) { *cpu->insn.reg0w = cpu->insn.imm0; }



void op_movrmib(CPU)
{
	if (cpu->insn.op_memory) bus_write8(&cpu->mem, cpu->insn.addr, cpu->insn.imm1);
	else                     *cpu->insn.reg1b = cpu->insn.imm1;
}



void op_movrmiw(CPU)
{
	if (cpu->insn.op_memory) bus_write16(&cpu->mem, cpu->insn.addr, cpu->insn.imm1);
	else                     *cpu->insn.reg1w = cpu->insn.imm1;
}



void op_movrrmb(CPU)
{

	if (cpu->insn.op_reverse) {

		if (cpu->insn.op_memory) bus_write8(&cpu->mem, cpu->insn.addr, *cpu->insn.reg0b);
		else                     *cpu->insn.reg1b = *cpu->insn.reg0b;

	} else {

		if (cpu->insn.op_memory) *cpu->insn.reg0b = bus_read8(&cpu->mem, cpu->insn.addr);
		else                     *cpu->insn.reg0b = *cpu->insn.reg1b;

	}

}



void op_movrrmw(CPU)
{

	if (cpu->insn.op_reverse) {

		if (cpu->insn.op_memory) bus_write16(&cpu->mem, cpu->insn.addr, *cpu->insn.reg0w);
		else                     *cpu->insn.reg1w = *cpu->insn.reg0w;

	} else {

		if (cpu->insn.op_memory) *cpu->insn.reg0w = bus_read16(&cpu->mem, cpu->insn.addr);
		else                     *cpu->insn.reg0w = *cpu->insn.reg1w;

	}

}



void op_movseg(CPU)
{

	if (cpu->insn.op_reverse) {

		if (cpu->insn.op_memory) bus_write16(&cpu->mem, cpu->insn.addr, *cpu->insn.reg0w);
		else                     *cpu->insn.reg1w = *cpu->insn.reg0w;

	} else {

		if (cpu->insn.op_memory) *cpu->insn.reg0w = bus_read16(&cpu->mem, cpu->insn.addr);
		else                     *cpu->insn.reg0w = *cpu->insn.reg1w;

	}

}



void op_sahf(CPU) {
	cpu->flags.w = (cpu->flags.w & 0xff00) | (cpu->regs.ax.h & 0xd5) | 0x02;
}



void op_xcharw(CPU)
{
	aluu tmp = cpu->regs.ax.w;
	cpu->regs.ax.w   = *cpu->insn.reg0w;
	*cpu->insn.reg0w = tmp;
}



void op_xchrmb(CPU)
{

	aluu tmp;

	if (cpu->insn.op_memory) {

		tmp = bus_read8(&cpu->mem, cpu->insn.addr);
		bus_write8(&cpu->mem, cpu->insn.addr, *cpu->insn.reg0b);

	} else {

		tmp = *cpu->insn.reg1b;
		*cpu->insn.reg1b = *cpu->insn.reg0b;

	}

	*cpu->insn.reg0b = tmp;

}



void op_xchrmw(CPU)
{

	uint tmp;

	if (cpu->insn.op_memory) {

		tmp = bus_read16(&cpu->mem, cpu->insn.addr);
		bus_write16(&cpu->mem, cpu->insn.addr, *cpu->insn.reg0w);

	} else {

		tmp = *cpu->insn.reg1w;
		*cpu->insn.reg1w = *cpu->insn.reg0w;

	}

	*cpu->insn.reg0w = tmp;

}



void op_xlatab(CPU)
{
	u32 addr = *get_rs(cpu, cpu->insn.segment) * 16 + ((cpu->regs.bx.w + cpu->regs.ax.l) & 0xffffu);
	cpu->regs.ax.l = bus_read8(&cpu->mem, addr);

}



void op_popcs( CPU) { cpu->regs.cs     = pop16(cpu); }
void op_popds( CPU) { cpu->regs.ds     = pop16(cpu); }
void op_popes( CPU) { cpu->regs.es     = pop16(cpu); }
void op_popss( CPU) { cpu->regs.ss     = pop16(cpu); }
void op_popf(  CPU) { cpu->flags.w     = (pop16(cpu) & 0x0ed5) | 0xf002; }
void op_poprw( CPU) { *cpu->insn.reg0w = pop16(cpu); }
void op_pushcs(CPU) { decsp16(cpu); pushx16(cpu, cpu->regs.cs); }
void op_pushds(CPU) { decsp16(cpu); pushx16(cpu, cpu->regs.ds); }
void op_pushes(CPU) { decsp16(cpu); pushx16(cpu, cpu->regs.es); }
void op_pushss(CPU) { decsp16(cpu); pushx16(cpu, cpu->regs.ss); }
void op_pushsp(CPU) { decsp16(cpu); pushx16(cpu, cpu->regs.sp.w); }
void op_pushf( CPU) { decsp16(cpu); pushx16(cpu, cpu->flags.w | 0x0002); }
void op_pushrw(CPU) { decsp16(cpu); pushx16(cpu, *cpu->insn.reg0w); }



void op_poprmw(CPU)
{

	aluu tmp = pop16(cpu);

	if (cpu->insn.op_memory) bus_write16(&cpu->mem, cpu->insn.addr, tmp);
	else                     *cpu->insn.reg1w = tmp;

}



void op_pushrmw(CPU)
{

	aluu tmp;

	decsp16(cpu);

	if (cpu->insn.op_memory) tmp = bus_read16(&cpu->mem, cpu->insn.addr);
	else                     tmp = *cpu->insn.reg1w;

	pushx16(cpu, tmp);

}



void op_callf(CPU)
{

	decsp16(cpu); pushx16(cpu, cpu->regs.cs);
	decsp16(cpu); pushx16(cpu, cpu->regs.ip);

	cpu->regs.ip = cpu->insn.imm0;
	cpu->regs.cs = cpu->insn.imm1;

}



void op_calln(CPU) { decsp16(cpu); pushx16(cpu, cpu->regs.ip); cpu->regs.ip += cpu->insn.imm0; }



void op_callfrm(CPU)
{

	if (!cpu->insn.op_memory)
		op_undef(cpu);

	aluu tip = bus_read16(&cpu->mem, cpu->insn.addr + 0);
	aluu tcs = bus_read16(&cpu->mem, cpu->insn.addr + 2);

	decsp16(cpu); pushx16(cpu, cpu->regs.cs);
	decsp16(cpu); pushx16(cpu, cpu->regs.ip);

	cpu->regs.ip = tip;
	cpu->regs.cs = tcs;

}



void op_callnrm(CPU)
{
	aluu tmp;

	if (cpu->insn.op_memory) tmp = bus_read16(&cpu->mem, cpu->insn.addr);
	else                     tmp = *cpu->insn.reg1w;

	decsp16(cpu);
	pushx16(cpu, cpu->regs.ip);

	cpu->regs.ip = tmp;

}


void op_int3(CPU)  { i8086_interrupt(cpu, I8086_VECTOR_BREAK); }
void op_into(CPU)  { if (cpu->flags.v) i8086_interrupt(cpu, I8086_VECTOR_VFLOW); }
void op_intib(CPU) { i8086_interrupt(cpu, cpu->insn.imm0); }



void op_iret(CPU)
{
	cpu->regs.ip = pop16(cpu);
	cpu->regs.cs = pop16(cpu);

	op_popf(cpu);

}

void op_jcbe(CPU) { if (cpu->flags.c                 || cpu->flags.z) cpu->regs.ip += cpu->insn.imm0; }
void op_jcle(CPU) { if (cpu->flags.s != cpu->flags.v || cpu->flags.z) cpu->regs.ip += cpu->insn.imm0; }
void op_jcl(CPU)  { if (cpu->flags.s != cpu->flags.v)                 cpu->regs.ip += cpu->insn.imm0; }

void op_jcc(CPU) { if (cpu->flags.c) cpu->regs.ip += cpu->insn.imm0; }
void op_jco(CPU) { if (cpu->flags.v) cpu->regs.ip += cpu->insn.imm0; }
void op_jcp(CPU) { if (cpu->flags.p) cpu->regs.ip += cpu->insn.imm0; }
void op_jcs(CPU) { if (cpu->flags.s) cpu->regs.ip += cpu->insn.imm0; }
void op_jcz(CPU) { if (cpu->flags.z) cpu->regs.ip += cpu->insn.imm0; }

void op_jcnbe(CPU) { if (!cpu->flags.c                && !cpu->flags.z) cpu->regs.ip += cpu->insn.imm0; }
void op_jcnle(CPU) { if (cpu->flags.s == cpu->flags.v && !cpu->flags.z) cpu->regs.ip += cpu->insn.imm0; }
void op_jcnl(CPU)  { if (cpu->flags.s == cpu->flags.v)                  cpu->regs.ip += cpu->insn.imm0; }

void op_jcnc(CPU) { if (!cpu->flags.c) cpu->regs.ip += cpu->insn.imm0; }
void op_jcno(CPU) { if (!cpu->flags.v) cpu->regs.ip += cpu->insn.imm0; }
void op_jcnp(CPU) { if (!cpu->flags.p) cpu->regs.ip += cpu->insn.imm0; }
void op_jcns(CPU) { if (!cpu->flags.s) cpu->regs.ip += cpu->insn.imm0; }
void op_jcnz(CPU) { if (!cpu->flags.z) cpu->regs.ip += cpu->insn.imm0; }

void op_jcxzr(CPU) { if (!cpu->regs.cx.w)  cpu->regs.ip += cpu->insn.imm0; }

void op_jmpf( CPU) { cpu->regs.ip = cpu->insn.imm0; cpu->regs.cs = cpu->insn.imm1; }
void op_jmpn( CPU) { cpu->regs.ip += cpu->insn.imm0; }

void op_jmpfrm(CPU)
{

	if (!cpu->insn.op_memory)
		op_undef(cpu);

	cpu->regs.ip = bus_read16(&cpu->mem, cpu->insn.addr + 0);
	cpu->regs.cs = bus_read16(&cpu->mem, cpu->insn.addr + 2);

}

void op_jmpnrm(CPU)
{
	if (cpu->insn.op_memory) cpu->regs.ip = bus_read16(&cpu->mem, cpu->insn.addr);
	else                     cpu->regs.ip = *cpu->insn.reg1w;
}


void op_loopnzr(CPU) { if (--cpu->regs.cx.w && !cpu->flags.z) cpu->regs.ip += cpu->insn.imm0; }
void op_loopzr(CPU)  { if (--cpu->regs.cx.w && cpu->flags.z)  cpu->regs.ip += cpu->insn.imm0; }
void op_loopr(CPU)   { if (--cpu->regs.cx.w)                  cpu->regs.ip += cpu->insn.imm0; }

void op_retf( CPU) { cpu->regs.ip = pop16(cpu); cpu->regs.cs = pop16(cpu); cpu->regs.sp.w += cpu->insn.imm0; }
void op_retn( CPU) { cpu->regs.ip = pop16(cpu); cpu->regs.sp.w += cpu->insn.imm0; }


void op_inib(CPU) { cpu->regs.ax.l = bus_read8( &cpu->io, cpu->insn.imm0 & 0xff); }
void op_iniw(CPU) { cpu->regs.ax.w = bus_read16(&cpu->io, cpu->insn.imm0 & 0xff); }
void op_inrb(CPU) { cpu->regs.ax.l = bus_read8( &cpu->io, cpu->regs.dx.w); }
void op_inrw(CPU) { cpu->regs.ax.w = bus_read16(&cpu->io, cpu->regs.dx.w); }

void op_outib(CPU) { bus_write8( &cpu->io, cpu->insn.imm0 & 0xff, cpu->regs.ax.l); }
void op_outiw(CPU) { bus_write16(&cpu->io, cpu->insn.imm0 & 0xff, cpu->regs.ax.w); }
void op_outrb(CPU) { bus_write8( &cpu->io, cpu->regs.dx.w, cpu->regs.ax.l); }
void op_outrw(CPU) { bus_write16(&cpu->io, cpu->regs.dx.w, cpu->regs.ax.w); }


void op_seges(CPU) { cpu->insn.segment = 0; }
void op_segcs(CPU) { cpu->insn.segment = 1; }
void op_segss(CPU) { cpu->insn.segment = 2; }
void op_segds(CPU) { cpu->insn.segment = 3; }
void op_lock( CPU) { }
void op_repnz(CPU) { cpu->insn.repeat_eq = false; cpu->insn.repeat_ne = true;  }
void op_repz( CPU) { cpu->insn.repeat_eq = true;  cpu->insn.repeat_ne = false; }
void op_wait( CPU) { /* FIXME: Not implemented */ }


void op_clc(CPU) { cpu->flags.c = false; }
void op_stc(CPU) { cpu->flags.c = true;  }
void op_cli(CPU) { cpu->flags.i = false; }
void op_cmc(CPU) { cpu->flags.c = !cpu->flags.c; }
void op_sti(CPU) { cpu->flags.i = true;  }
void op_cld(CPU) { cpu->flags.d = false; }
void op_std(CPU) { cpu->flags.d = true;  }



void op_cmpsb(CPU)
{

	if ((cpu->insn.repeat_eq || cpu->insn.repeat_ne) && (cpu->regs.cx.w == 0))
		return;

	u32  addr = *get_rs(cpu, cpu->insn.segment) * 16 + cpu->regs.si.w;
	uint tmpa = bus_read8(&cpu->mem, addr);
	uint tmpb = bus_read8(&cpu->mem, cpu->regs.es * 16 + cpu->regs.di.w);

	sub8(cpu, tmpa, tmpb);

	if (cpu->flags.d) {

		cpu->regs.si.w--;
		cpu->regs.di.w--;

	} else {

		cpu->regs.si.w++;
		cpu->regs.di.w++;
	}

	cpu->insn.complete =
		cpu->insn.repeat_eq? (--cpu->regs.cx.w == 0) || !cpu->flags.z:
		cpu->insn.repeat_ne? (--cpu->regs.cx.w == 0) || cpu->flags.z:
		true;

}



void op_cmpsw(CPU)
{

	if ((cpu->insn.repeat_eq || cpu->insn.repeat_ne) && (cpu->regs.cx.w == 0))
		return;

	u32  addr = *get_rs(cpu, cpu->insn.segment) * 16 + cpu->regs.si.w;
	uint tmpa = bus_read16(&cpu->mem, addr);
	uint tmpb = bus_read16(&cpu->mem, cpu->regs.es * 16 + cpu->regs.di.w);

	sub16(cpu, tmpa, tmpb);

	if (cpu->flags.d) {

		cpu->regs.si.w -= 2;
		cpu->regs.di.w -= 2;

	} else {

		cpu->regs.si.w += 2;
		cpu->regs.di.w += 2;
	}

	cpu->insn.complete =
		cpu->insn.repeat_eq? (--cpu->regs.cx.w == 0) || !cpu->flags.z:
		cpu->insn.repeat_ne? (--cpu->regs.cx.w == 0) || cpu->flags.z:
		true;

}



void op_lodsb(CPU)
{

	if ((cpu->insn.repeat_eq || cpu->insn.repeat_ne) && (cpu->regs.cx.w == 0))
		return;

	u32 addr = *get_rs(cpu, cpu->insn.segment) * 16 + cpu->regs.si.w;
	cpu->regs.ax.l = bus_read8(&cpu->mem, addr);

	if (cpu->flags.d) cpu->regs.si.w--;
	else              cpu->regs.si.w++;

	cpu->insn.complete = !(cpu->insn.repeat_eq || cpu->insn.repeat_ne) || (--cpu->regs.cx.w == 0);

}



void op_lodsw(CPU)
{

	if ((cpu->insn.repeat_eq || cpu->insn.repeat_ne) && (cpu->regs.cx.w == 0))
		return;

	u32 addr = *get_rs(cpu, cpu->insn.segment) * 16 + cpu->regs.si.w;
	cpu->regs.ax.w = bus_read16(&cpu->mem, addr);

	if (cpu->flags.d) cpu->regs.si.w -= 2;
	else              cpu->regs.si.w += 2;

	cpu->insn.complete = !(cpu->insn.repeat_eq || cpu->insn.repeat_ne) || (--cpu->regs.cx.w == 0);

}



void op_movsb(CPU)
{

	if ((cpu->insn.repeat_eq || cpu->insn.repeat_ne) && (cpu->regs.cx.w == 0))
		return;

	u32  addr = *get_rs(cpu, cpu->insn.segment) * 16 + cpu->regs.si.w;
	aluu tmp  = bus_read8(&cpu->mem, addr);

	bus_write8(&cpu->mem, cpu->regs.es * 16 + cpu->regs.di.w, tmp);

	if (cpu->flags.d) {

		cpu->regs.si.w--;
		cpu->regs.di.w--;

	} else {

		cpu->regs.si.w++;
		cpu->regs.di.w++;
	}

	cpu->insn.complete = !(cpu->insn.repeat_eq || cpu->insn.repeat_ne) || (--cpu->regs.cx.w == 0);

}



void op_movsw(CPU)
{

	if ((cpu->insn.repeat_eq || cpu->insn.repeat_ne) && (cpu->regs.cx.w == 0))
		return;

	u32  addr = *get_rs(cpu, cpu->insn.segment) * 16 + cpu->regs.si.w;
	aluu tmp  = bus_read16(&cpu->mem, addr);

	bus_write16(&cpu->mem, cpu->regs.es * 16 + cpu->regs.di.w, tmp);

	if (cpu->flags.d) {

		cpu->regs.si.w -= 2;
		cpu->regs.di.w -= 2;

	} else {

		cpu->regs.si.w += 2;
		cpu->regs.di.w += 2;
	}

	cpu->insn.complete = !(cpu->insn.repeat_eq || cpu->insn.repeat_ne) || (--cpu->regs.cx.w == 0);

}



void op_scasb(CPU)
{

	if ((cpu->insn.repeat_eq || cpu->insn.repeat_ne) && (cpu->regs.cx.w == 0))
		return;

	u32 addr = cpu->regs.es * 16 + cpu->regs.di.w;
	sub8(cpu, cpu->regs.ax.l, bus_read8(&cpu->mem, addr));

	if (cpu->flags.d) cpu->regs.di.w--;
	else              cpu->regs.di.w++;

	if      (cpu->insn.repeat_eq) { cpu->insn.complete = (--cpu->regs.cx.w == 0) || !cpu->flags.z; }
	else if (cpu->insn.repeat_ne) { cpu->insn.complete = (--cpu->regs.cx.w == 0) ||  cpu->flags.z; }

}



void op_scasw(CPU)
{

	if ((cpu->insn.repeat_eq || cpu->insn.repeat_ne) && (cpu->regs.cx.w == 0))
		return;

	u32 addr = cpu->regs.es * 16 + cpu->regs.di.w;
	sub16(cpu, cpu->regs.ax.w, bus_read16(&cpu->mem, addr));

	if (cpu->flags.d) cpu->regs.di.w -= 2;
	else              cpu->regs.di.w += 2;

	if      (cpu->insn.repeat_eq) { cpu->insn.complete = (--cpu->regs.cx.w == 0) || !cpu->flags.z; }
	else if (cpu->insn.repeat_ne) { cpu->insn.complete = (--cpu->regs.cx.w == 0) ||  cpu->flags.z; }

}



void op_stosb(CPU)
{

	if ((cpu->insn.repeat_eq || cpu->insn.repeat_ne) && (cpu->regs.cx.w == 0))
		return;

	bus_write8(&cpu->mem, cpu->regs.es * 16 + cpu->regs.di.w, cpu->regs.ax.l);

	if (cpu->flags.d) cpu->regs.di.w--;
	else              cpu->regs.di.w++;

	cpu->insn.complete = !(cpu->insn.repeat_eq || cpu->insn.repeat_ne) || (--cpu->regs.cx.w == 0);

}



void op_stosw(CPU)
{

	if ((cpu->insn.repeat_eq || cpu->insn.repeat_ne) && (cpu->regs.cx.w == 0))
		return;

	bus_write16(&cpu->mem, cpu->regs.es * 16 + cpu->regs.di.w, cpu->regs.ax.w);

	if (cpu->flags.d) cpu->regs.di.w -= 2;
	else              cpu->regs.di.w += 2;

	cpu->insn.complete = !(cpu->insn.repeat_eq || cpu->insn.repeat_ne) || (--cpu->regs.cx.w == 0);

}



void op_fpu(CPU) { /* FIXME: No FPU implemented */ }



void op_undef(CPU)
{

	i8086_undef(cpu);

}

