

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "core/types.h"
#include "core/debug.h"
#include "core/bus.h"
#include "core/wire.h"

#include "cpu/i8086.h"



#define CPU  struct i8086 *cpu


typedef u32 aluu;
typedef i32 alui;


#define INCIPB()  do { cpu->regs.ip++;    } while (0)
#define INCIPW()  do { cpu->regs.ip += 2; } while (0)

#define ADVSP(x)  do { cpu->regs.sp.w += (x); } while (0)

#define ADVSIB()  do { if (cpu->flags.d) cpu->regs.si.w++;    else cpu->regs.si.w--;    } while (0)
#define ADVDIB()  do { if (cpu->flags.d) cpu->regs.di.w++;    else cpu->regs.di.w--;    } while (0)
#define ADVSIW()  do { if (cpu->flags.d) cpu->regs.si.w += 2; else cpu->regs.si.w -= 2; } while (0)
#define ADVDIW()  do { if (cpu->flags.d) cpu->regs.di.w += 2; else cpu->regs.di.w -= 2; } while (0)

#define LDIPUB()       bus_read8( &cpu->mem, cpu->regs.cs * 16 + cpu->regs.ip); INCIPB()
#define LDIPIB()   (i8)bus_read8( &cpu->mem, cpu->regs.cs * 16 + cpu->regs.ip); INCIPB()
#define LDIPUW()       bus_read16(&cpu->mem, cpu->regs.cs * 16 + cpu->regs.ip); INCIPW()
#define LDIPIW()  (i16)bus_read16(&cpu->mem, cpu->regs.cs * 16 + cpu->regs.ip); INCIPW()

#define LDEAR0MB()  ((cpu->insn.op_memory)? bus_read8( &cpu->mem, cpu->insn.addr): *cpu->insn.reg0b)
#define LDEAR1MB()  ((cpu->insn.op_memory)? bus_read8( &cpu->mem, cpu->insn.addr): *cpu->insn.reg1b)
#define LDEAR0MW()  ((cpu->insn.op_memory)? bus_read16(&cpu->mem, cpu->insn.addr): *cpu->insn.reg0w)
#define LDEAR1MW()  ((cpu->insn.op_memory)? bus_read16(&cpu->mem, cpu->insn.addr): *cpu->insn.reg1w)

#define LDSU0B()  do { cpu->insn.imm0 =      bus_read8( &cpu->mem, cpu->regs.cs * 16 + cpu->regs.ip); INCIPB(); } while (0)
#define LDSU1B()  do { cpu->insn.imm1 =      bus_read8( &cpu->mem, cpu->regs.cs * 16 + cpu->regs.ip); INCIPB(); } while (0)
#define LDSI0B()  do { cpu->insn.imm0 =  (i8)bus_read8( &cpu->mem, cpu->regs.cs * 16 + cpu->regs.ip); INCIPB(); } while (0)
#define LDSI1B()  do { cpu->insn.imm1 =  (i8)bus_read8( &cpu->mem, cpu->regs.cs * 16 + cpu->regs.ip); INCIPB(); } while (0)
#define LDSU0W()  do { cpu->insn.imm0 =      bus_read16(&cpu->mem, cpu->regs.cs * 16 + cpu->regs.ip); INCIPW(); } while (0)
#define LDSU1W()  do { cpu->insn.imm1 =      bus_read16(&cpu->mem, cpu->regs.cs * 16 + cpu->regs.ip); INCIPW(); } while (0)
#define LDSI0W()  do { cpu->insn.imm0 = (i16)bus_read16(&cpu->mem, cpu->regs.cs * 16 + cpu->regs.ip); INCIPW(); } while (0)
#define LDSI1W()  do { cpu->insn.imm1 = (i16)bus_read16(&cpu->mem, cpu->regs.cs * 16 + cpu->regs.ip); INCIPW(); } while (0)

#define LDSPB(n)  bus_read8( &cpu->mem, cpu->regs.ss * 16 + cpu->regs.sp.w + (n))
#define LDSPW(n)  bus_read16(&cpu->mem, cpu->regs.ss * 16 + cpu->regs.sp.w + (n))

#define STSPB(n, x)  do { bus_write8( &cpu->mem, cpu->regs.ss * 16 + cpu->regs.sp.w + (n), (x)); } while (0)
#define STSPW(n, x)  do { bus_write16(&cpu->mem, cpu->regs.ss * 16 + cpu->regs.sp.w + (n), (x)); } while (0)

#define STEAR0MB(x) do { if (cpu->insn.op_memory) bus_write8( &cpu->mem, cpu->insn.addr, (x)); else *cpu->insn.reg0b = (x); } while (0)
#define STEAR1MB(x) do { if (cpu->insn.op_memory) bus_write8( &cpu->mem, cpu->insn.addr, (x)); else *cpu->insn.reg1b = (x); } while (0)
#define STEAR0MW(x) do { if (cpu->insn.op_memory) bus_write16(&cpu->mem, cpu->insn.addr, (x)); else *cpu->insn.reg0w = (x); } while (0)
#define STEAR1MW(x) do { if (cpu->insn.op_memory) bus_write16(&cpu->mem, cpu->insn.addr, (x)); else *cpu->insn.reg1w = (x); } while (0)



static inline u8  *get_r8(CPU, uint r)  { return ((r & 0x04)? &cpu->regs.ax.h: &cpu->regs.ax.l) + 2 * (r & 0x03); }
static inline u16 *get_r16(CPU, uint r) { return &cpu->regs.ax.w + (r & 0x07); }
static inline u16 *get_rs(CPU, uint r)  { return &cpu->regs.es   + (r & 0x03); }


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

static inline aluu aluaddop(CPU, uint m, aluu x, aluu a, aluu b, bool d) {
	cpu->flags.c = carry(x, m);
	cpu->flags.v = vflow(x, a, b, m);
	cpu->flags.p = parity(x);
	cpu->flags.a = d ^ sign(x ^ a ^ b, 0x10);
	cpu->flags.z = zero(x, m);
	cpu->flags.s = sign(x, m);
	return x;
}

static inline aluu aluincop(CPU, uint m, aluu x, aluu a, aluu b, bool d) { //Does not affect carry
	cpu->flags.v = vflow(x, a, b, m);
	cpu->flags.p = parity(x);
	cpu->flags.a = d ^ sign(x ^ a ^ b, 0x10);
	cpu->flags.z = zero(x, m);
	cpu->flags.s = sign(x, m);
	return x;
}

static inline aluu alulogop(CPU, uint m, aluu x) { // C=0, V=0, A=0
	cpu->flags.c = false;
	cpu->flags.v = false;
	cpu->flags.p = parity(x);
	cpu->flags.a = false;
	cpu->flags.z = zero(x, m);
	cpu->flags.s = sign(x, m);
	return x;
}


static inline aluu add8( CPU, aluu a, aluu b) { return aluaddop(cpu, 0x80,   a + b,                a, b, false); }
static inline aluu add16(CPU, aluu a, aluu b) { return aluaddop(cpu, 0x8000, a + b,                a, b, false); }
static inline aluu adc8( CPU, aluu a, aluu b) { return aluaddop(cpu, 0x80,   a + b + cpu->flags.c, a, b, false); }
static inline aluu adc16(CPU, aluu a, aluu b) { return aluaddop(cpu, 0x8000, a + b + cpu->flags.c, a, b, false); }

static inline aluu sub8( CPU, aluu a, aluu b) { return aluaddop(cpu, 0x80,   a - b,                a, ~b, true); }
static inline aluu sub16(CPU, aluu a, aluu b) { return aluaddop(cpu, 0x8000, a - b,                a, ~b, true); }
static inline aluu sbb8( CPU, aluu a, aluu b) { return aluaddop(cpu, 0x80,   a - b - cpu->flags.c, a, ~b, true); }
static inline aluu sbb16(CPU, aluu a, aluu b) { return aluaddop(cpu, 0x8000, a - b - cpu->flags.c, a, ~b, true); }

static inline aluu inc8( CPU, aluu a) { return aluincop(cpu, 0x80,   a + 1, a,  1, false); }
static inline aluu inc16(CPU, aluu a) { return aluincop(cpu, 0x8000, a + 1, a,  1, false); }
static inline aluu dec8( CPU, aluu a) { return aluincop(cpu, 0x80,   a - 1, a, ~1, true);  }
static inline aluu dec16(CPU, aluu a) { return aluincop(cpu, 0x8000, a - 1, a, ~1, true);  }

static inline aluu and8(CPU, aluu a, aluu b) { return alulogop(cpu, 0x80, a & b); }
static inline aluu ior8(CPU, aluu a, aluu b) { return alulogop(cpu, 0x80, a | b); }
static inline aluu xor8(CPU, aluu a, aluu b) { return alulogop(cpu, 0x80, a ^ b); }

static inline aluu and16(CPU, aluu a, u16 b) { return alulogop(cpu, 0x8000, a & b); }
static inline aluu ior16(CPU, aluu a, u16 b) { return alulogop(cpu, 0x8000, a | b); }
static inline aluu xor16(CPU, aluu a, u16 b) { return alulogop(cpu, 0x8000, a ^ b); }

static inline u8 shrb(u8 a)         { return a >> 1; }
static inline u8 shlb(u8 a)         { return a << 1; }
static inline u8 sarb(u8 a)         { return (i8)a >> 1; }
static inline u8 salb(u8 a)         { return (i8)a << 1; }
static inline u8 rorb(u8 a)         { return (a >> 1) | (a << 7); }
static inline u8 rolb(u8 a)         { return (a << 1) | (a >> 7); }
static inline u8 rcrb(u8 a, bool c) { return (a >> 1) | (a << 8) | (c << 7); }
static inline u8 rclb(u8 a, bool c) { return (a << 1) | (a >> 8) | c; }

static inline aluu shrw(u16 a)         { return a >> 1; }
static inline aluu shlw(u16 a)         { return a << 1; }
static inline aluu sarw(u16 a)         { return (i16)a >> 1; }
static inline aluu salw(u16 a)         { return (i16)a << 1; }
static inline aluu rorw(u16 a)         { return (a >> 1) | (a << 15); }
static inline aluu rolw(u16 a)         { return (a << 1) | (a >> 15); }
static inline aluu rcrw(u16 a, bool c) { return (a >> 1) | (a << 16) | (c << 15); }
static inline aluu rclw(u16 a, bool c) { return (a << 1) | (a >> 16) | c; }


aluu rol8(CPU, aluu a, uint b, bool v)
{

	u8   x = a;
	bool c = cpu->flags.c;

	while (true)
		switch (b) {

			case 7: c = sign(x, 0x80); x = rolb(x);
			case 6: c = sign(x, 0x80); x = rolb(x);
			case 5: c = sign(x, 0x80); x = rolb(x);
			case 4: c = sign(x, 0x80); x = rolb(x);
			case 3: c = sign(x, 0x80); x = rolb(x);
			case 2: c = sign(x, 0x80); x = rolb(x);
			case 1: c = sign(x, 0x80); x = rolb(x);
			case 0: goto done;

			default:
				c = sign(x, 0x01);
				b -= 8;

		}

done:
	cpu->flags.c = c;

	if (v)
		cpu->flags.v = sign(x, 0x80) ^ c;

	return x;

}



aluu ror8(CPU, aluu a, uint b, bool v)
{

	u8   x = a;
	bool c = cpu->flags.c;

	while (true)
		switch (b) {

			case 7: c = sign(x, 0x01); x = rorb(x);
			case 6: c = sign(x, 0x01); x = rorb(x);
			case 5: c = sign(x, 0x01); x = rorb(x);
			case 4: c = sign(x, 0x01); x = rorb(x);
			case 3: c = sign(x, 0x01); x = rorb(x);
			case 2: c = sign(x, 0x01); x = rorb(x);
			case 1: c = sign(x, 0x01); x = rorb(x);
			case 0: goto done;

			default:
				c = sign(x, 0x80);
				b -= 8;

		}

done:
	cpu->flags.c = c;

	if (v)
		cpu->flags.v = sign(x, 0x80) ^ sign(x, 0x40);

	return x;

}



aluu rol16(CPU, aluu a, uint b, bool v)
{

	u16  x = a;
	bool c = cpu->flags.c;

	while (true)
		switch (b) {

			case 15: c = sign(x, 0x8000); x = rolw(x);
			case 14: c = sign(x, 0x8000); x = rolw(x);
			case 13: c = sign(x, 0x8000); x = rolw(x);
			case 12: c = sign(x, 0x8000); x = rolw(x);
			case 11: c = sign(x, 0x8000); x = rolw(x);
			case 10: c = sign(x, 0x8000); x = rolw(x);
			case  9: c = sign(x, 0x8000); x = rolw(x);
			case  8: c = sign(x, 0x8000); x = rolw(x);
			case  7: c = sign(x, 0x8000); x = rolw(x);
			case  6: c = sign(x, 0x8000); x = rolw(x);
			case  5: c = sign(x, 0x8000); x = rolw(x);
			case  4: c = sign(x, 0x8000); x = rolw(x);
			case  3: c = sign(x, 0x8000); x = rolw(x);
			case  2: c = sign(x, 0x8000); x = rolw(x);
			case  1: c = sign(x, 0x8000); x = rolw(x);
			case  0: goto done;

			default:
				c = sign(x, 0x0001);
				b -= 16;

		}

done:
	cpu->flags.c = c;

	if (v)
		cpu->flags.v = sign(x, 0x8000) ^ c;

	return x;

}



aluu ror16(CPU, aluu a, uint b, bool v)
{

	u16  x = a;
	bool c = cpu->flags.c;

	while (true)
		switch (b) {

			case 15: c = sign(x, 0x0001); x = rorw(x);
			case 14: c = sign(x, 0x0001); x = rorw(x);
			case 13: c = sign(x, 0x0001); x = rorw(x);
			case 12: c = sign(x, 0x0001); x = rorw(x);
			case 11: c = sign(x, 0x0001); x = rorw(x);
			case 10: c = sign(x, 0x0001); x = rorw(x);
			case  9: c = sign(x, 0x0001); x = rorw(x);
			case  8: c = sign(x, 0x0001); x = rorw(x);
			case  7: c = sign(x, 0x0001); x = rorw(x);
			case  6: c = sign(x, 0x0001); x = rorw(x);
			case  5: c = sign(x, 0x0001); x = rorw(x);
			case  4: c = sign(x, 0x0001); x = rorw(x);
			case  3: c = sign(x, 0x0001); x = rorw(x);
			case  2: c = sign(x, 0x0001); x = rorw(x);
			case  1: c = sign(x, 0x0001); x = rorw(x);
			case  0: goto done;

			default:
				c = sign(x, 0x8000);
				b -= 16;

		}

done:
	cpu->flags.c = c;

	if (v)
		cpu->flags.v = sign(x, 0x8000) ^ sign(x, 0x4000);

	return x;

}




aluu rcl8(CPU, aluu a, uint b, bool v)
{

	u8   x  = a;
	bool ci = cpu->flags.c;
	bool co = ci;

	while (true)
		switch (b) {

			case 8: co = sign(x, 0x80); x = rclb(x, ci); ci = co;
			case 7: co = sign(x, 0x80); x = rclb(x, ci); ci = co;
			case 6: co = sign(x, 0x80); x = rclb(x, ci); ci = co;
			case 5: co = sign(x, 0x80); x = rclb(x, ci); ci = co;
			case 4: co = sign(x, 0x80); x = rclb(x, ci); ci = co;
			case 3: co = sign(x, 0x80); x = rclb(x, ci); ci = co;
			case 2: co = sign(x, 0x80); x = rclb(x, ci); ci = co;
			case 1: co = sign(x, 0x80); x = rclb(x, ci); ci = co;
			case 0: goto done;

			default:
				b -= 9;

		}

done:
	cpu->flags.c = co;

	if (v)
		cpu->flags.v = sign(x, 0x80) ^ co;

	return x;

}



aluu rcr8(CPU, aluu a, uint b, bool v)
{

	u8   x  = a;
	bool ci = cpu->flags.c;
	bool co = ci;

	while (true)
		switch (b) {

			case 8: co = sign(x, 0x01); x = rcrb(x, ci); ci = co;
			case 7: co = sign(x, 0x01); x = rcrb(x, ci); ci = co;
			case 6: co = sign(x, 0x01); x = rcrb(x, ci); ci = co;
			case 5: co = sign(x, 0x01); x = rcrb(x, ci); ci = co;
			case 4: co = sign(x, 0x01); x = rcrb(x, ci); ci = co;
			case 3: co = sign(x, 0x01); x = rcrb(x, ci); ci = co;
			case 2: co = sign(x, 0x01); x = rcrb(x, ci); ci = co;
			case 1: co = sign(x, 0x01); x = rcrb(x, ci); ci = co;
			case 0: goto done;

			default:
				b -= 9;

		}

done:
	cpu->flags.c = co;

	if (v)
		cpu->flags.v = sign(x, 0x80) ^ sign(x, 0x40);

	return x;

}



aluu rcl16(CPU, aluu a, uint b, bool v)
{

	u16  x  = a;
	bool ci = cpu->flags.c;
	bool co = ci;

	while (true)
		switch (b) {

			case 16: co = sign(x, 0x8000); x = rclw(x, ci); ci = co;
			case 15: co = sign(x, 0x8000); x = rclw(x, ci); ci = co;
			case 14: co = sign(x, 0x8000); x = rclw(x, ci); ci = co;
			case 13: co = sign(x, 0x8000); x = rclw(x, ci); ci = co;
			case 12: co = sign(x, 0x8000); x = rclw(x, ci); ci = co;
			case 11: co = sign(x, 0x8000); x = rclw(x, ci); ci = co;
			case 10: co = sign(x, 0x8000); x = rclw(x, ci); ci = co;
			case  9: co = sign(x, 0x8000); x = rclw(x, ci); ci = co;
			case  8: co = sign(x, 0x8000); x = rclw(x, ci); ci = co;
			case  7: co = sign(x, 0x8000); x = rclw(x, ci); ci = co;
			case  6: co = sign(x, 0x8000); x = rclw(x, ci); ci = co;
			case  5: co = sign(x, 0x8000); x = rclw(x, ci); ci = co;
			case  4: co = sign(x, 0x8000); x = rclw(x, ci); ci = co;
			case  3: co = sign(x, 0x8000); x = rclw(x, ci); ci = co;
			case  2: co = sign(x, 0x8000); x = rclw(x, ci); ci = co;
			case  1: co = sign(x, 0x8000); x = rclw(x, ci); ci = co;
			case  0: goto done;

			default:
				b -= 17;

		}

done:
	cpu->flags.c = co;

	if (v)
		cpu->flags.v = sign(x, 0x8000) ^ co;

	return x;

}



aluu rcr16(CPU, aluu a, uint b, bool v)
{

	u16  x  = a;
	bool ci = cpu->flags.c;
	bool co = ci;

	while (true)
		switch (b) {

			case 16: co = sign(x, 0x0001); x = rcrw(x, ci); ci = co;
			case 15: co = sign(x, 0x0001); x = rcrw(x, ci); ci = co;
			case 14: co = sign(x, 0x0001); x = rcrw(x, ci); ci = co;
			case 13: co = sign(x, 0x0001); x = rcrw(x, ci); ci = co;
			case 12: co = sign(x, 0x0001); x = rcrw(x, ci); ci = co;
			case 11: co = sign(x, 0x0001); x = rcrw(x, ci); ci = co;
			case 10: co = sign(x, 0x0001); x = rcrw(x, ci); ci = co;
			case  9: co = sign(x, 0x0001); x = rcrw(x, ci); ci = co;
			case  8: co = sign(x, 0x0001); x = rcrw(x, ci); ci = co;
			case  7: co = sign(x, 0x0001); x = rcrw(x, ci); ci = co;
			case  6: co = sign(x, 0x0001); x = rcrw(x, ci); ci = co;
			case  5: co = sign(x, 0x0001); x = rcrw(x, ci); ci = co;
			case  4: co = sign(x, 0x0001); x = rcrw(x, ci); ci = co;
			case  3: co = sign(x, 0x0001); x = rcrw(x, ci); ci = co;
			case  2: co = sign(x, 0x0001); x = rcrw(x, ci); ci = co;
			case  1: co = sign(x, 0x0001); x = rcrw(x, ci); ci = co;
			case  0: goto done;

			default:
				b -= 17;

		}

done:
	cpu->flags.c = co;

	if (v)
		cpu->flags.v = sign(x, 0x8000) ^ sign(x, 0x4000);

	return x;

}



aluu shl8(CPU, aluu a, uint b, bool v)
{

	u8   x = a;
	bool c = cpu->flags.c;

	switch (b) {

		case 7: c = sign(x, 0x80); x = shlb(x);
		case 6: c = sign(x, 0x80); x = shlb(x);
		case 5: c = sign(x, 0x80); x = shlb(x);
		case 4: c = sign(x, 0x80); x = shlb(x);
		case 3: c = sign(x, 0x80); x = shlb(x);
		case 2: c = sign(x, 0x80); x = shlb(x);
		case 1: c = sign(x, 0x80); x = shlb(x);
		case 0: goto done;

		default:
			x = 0;

	}

done:
	if (v) {

		cpu->flags.c = c;
		cpu->flags.v = sign(x, 0x80) ^ c;

	}

	if (b > 0) {

		cpu->flags.p = parity(x);
		cpu->flags.z = zero(x, 0x80);
		cpu->flags.s = sign(x, 0x80);

	}

	return x;

}



aluu shr8(CPU, aluu a, uint b, bool v)
{

	u8   x = a;
	bool c = cpu->flags.c;

	switch (b) {

		case 7: c = sign(x, 0x01); x = shrb(x);
		case 6: c = sign(x, 0x01); x = shrb(x);
		case 5: c = sign(x, 0x01); x = shrb(x);
		case 4: c = sign(x, 0x01); x = shrb(x);
		case 3: c = sign(x, 0x01); x = shrb(x);
		case 2: c = sign(x, 0x01); x = shrb(x);
		case 1: c = sign(x, 0x01); x = shrb(x);
		case 0: goto done;

		default:
			x = 0;

	}

done:
	if (v) {

		cpu->flags.c = c;
		cpu->flags.v = sign(x, 0x80) ^ sign(x, 0x40);

	}

	if (b > 0) {

		cpu->flags.p = parity(x);
		cpu->flags.z = zero(x, 0x80);
		cpu->flags.s = sign(x, 0x80);

	}

	return x;

}



aluu shl16(CPU, aluu a, uint b, bool v)
{

	u16  x = a;
	bool c = cpu->flags.c;

	switch (b) {

		case 15: c = sign(x, 0x8000); x = shlw(x);
		case 14: c = sign(x, 0x8000); x = shlw(x);
		case 13: c = sign(x, 0x8000); x = shlw(x);
		case 12: c = sign(x, 0x8000); x = shlw(x);
		case 11: c = sign(x, 0x8000); x = shlw(x);
		case 10: c = sign(x, 0x8000); x = shlw(x);
		case  9: c = sign(x, 0x8000); x = shlw(x);
		case  8: c = sign(x, 0x8000); x = shlw(x);
		case  7: c = sign(x, 0x8000); x = shlw(x);
		case  6: c = sign(x, 0x8000); x = shlw(x);
		case  5: c = sign(x, 0x8000); x = shlw(x);
		case  4: c = sign(x, 0x8000); x = shlw(x);
		case  3: c = sign(x, 0x8000); x = shlw(x);
		case  2: c = sign(x, 0x8000); x = shlw(x);
		case  1: c = sign(x, 0x8000); x = shlw(x);
		case  0: goto done;

		default:
			x = 0;

	}

done:
	if (v) {

		cpu->flags.c = c;
		cpu->flags.v = sign(x, 0x8000) ^ c;

	}

	if (b > 0) {

		cpu->flags.p = parity(x);
		cpu->flags.z = zero(x, 0x8000);
		cpu->flags.s = sign(x, 0x8000);

	}

	return x;

}



aluu shr16(CPU, aluu a, uint b, bool v)
{

	u16  x = a;
	bool c = cpu->flags.c;

	switch (b) {

		case 15: c = sign(x, 0x0001); x = shrw(x);
		case 14: c = sign(x, 0x0001); x = shrw(x);
		case 13: c = sign(x, 0x0001); x = shrw(x);
		case 12: c = sign(x, 0x0001); x = shrw(x);
		case 11: c = sign(x, 0x0001); x = shrw(x);
		case 10: c = sign(x, 0x0001); x = shrw(x);
		case  9: c = sign(x, 0x0001); x = shrw(x);
		case  8: c = sign(x, 0x0001); x = shrw(x);
		case  7: c = sign(x, 0x0001); x = shrw(x);
		case  6: c = sign(x, 0x0001); x = shrw(x);
		case  5: c = sign(x, 0x0001); x = shrw(x);
		case  4: c = sign(x, 0x0001); x = shrw(x);
		case  3: c = sign(x, 0x0001); x = shrw(x);
		case  2: c = sign(x, 0x0001); x = shrw(x);
		case  1: c = sign(x, 0x0001); x = shrw(x);
		case  0: goto done;

		default:
			x = 0;

	}

done:
	if (v) {

		cpu->flags.c = c;
		cpu->flags.v = sign(x, 0x8000) ^ sign(x, 0x4000);

	}

	if (b > 0) {

		cpu->flags.p = parity(x);
		cpu->flags.a = false;
		cpu->flags.z = zero(x, 0x8000);
		cpu->flags.s = sign(x, 0x8000);

	}

	return x;

}



aluu sal8(CPU, aluu a, uint b, bool v)
{

	u8   x = a;
	bool c = cpu->flags.c;

	switch (b) {

		case 7: c = sign(x, 0x80); x = salb(x);
		case 6: c = sign(x, 0x80); x = salb(x);
		case 5: c = sign(x, 0x80); x = salb(x);
		case 4: c = sign(x, 0x80); x = salb(x);
		case 3: c = sign(x, 0x80); x = salb(x);
		case 2: c = sign(x, 0x80); x = salb(x);
		case 1: c = sign(x, 0x80); x = salb(x);
		case 0: goto done;

		default:
			x = 0;

	}

done:
	if (v) {

		cpu->flags.c = c;
		cpu->flags.v = sign(x, 0x80) ^ c;

	}

	if (b > 0) {

		cpu->flags.p = parity(x);
		cpu->flags.z = zero(x, 0x80);
		cpu->flags.s = sign(x, 0x80);

	}

	return x;

}



aluu sar8(CPU, aluu a, uint b, bool v)
{

	u8   x = a;
	bool c = cpu->flags.c;

	switch (b) {

		case 7: c = sign(x, 0x01); x = sarb(x);
		case 6: c = sign(x, 0x01); x = sarb(x);
		case 5: c = sign(x, 0x01); x = sarb(x);
		case 4: c = sign(x, 0x01); x = sarb(x);
		case 3: c = sign(x, 0x01); x = sarb(x);
		case 2: c = sign(x, 0x01); x = sarb(x);
		case 1: c = sign(x, 0x01); x = sarb(x);
		case 0: goto done;

		default:
			x = sign(x, 0x80)? 0xff: 0;

	}

done:
	if (v) {

		cpu->flags.c = c;
		cpu->flags.v = sign(x, 0x80) ^ sign(x, 0x40);

	}

	if (b > 0) {

		cpu->flags.p = parity(x);
		cpu->flags.z = zero(x, 0x80);
		cpu->flags.s = sign(x, 0x80);

	}

	return x;

}



aluu sal16(CPU, aluu a, uint b, bool v)
{

	u16  x = a;
	bool c = cpu->flags.c;

	switch (b) {

		case 15: c = sign(x, 0x8000); x = salw(x);
		case 14: c = sign(x, 0x8000); x = salw(x);
		case 13: c = sign(x, 0x8000); x = salw(x);
		case 12: c = sign(x, 0x8000); x = salw(x);
		case 11: c = sign(x, 0x8000); x = salw(x);
		case 10: c = sign(x, 0x8000); x = salw(x);
		case  9: c = sign(x, 0x8000); x = salw(x);
		case  8: c = sign(x, 0x8000); x = salw(x);
		case  7: c = sign(x, 0x8000); x = salw(x);
		case  6: c = sign(x, 0x8000); x = salw(x);
		case  5: c = sign(x, 0x8000); x = salw(x);
		case  4: c = sign(x, 0x8000); x = salw(x);
		case  3: c = sign(x, 0x8000); x = salw(x);
		case  2: c = sign(x, 0x8000); x = salw(x);
		case  1: c = sign(x, 0x8000); x = salw(x);
		case  0: goto done;

		default:
			x = 0;

	}

done:
	if (v) {

		cpu->flags.c = c;
		cpu->flags.v = sign(x, 0x8000) ^ c;

	}

	if (b > 0) {

		cpu->flags.p = parity(x);
		cpu->flags.z = zero(x, 0x8000);
		cpu->flags.s = sign(x, 0x8000);

	}

	return x;

}



aluu sar16(CPU, aluu a, uint b, bool v)
{

	u16  x = a;
	bool c = cpu->flags.c;

	switch (b) {

		case 15: c = sign(x, 0x0001); x = sarw(x);
		case 14: c = sign(x, 0x0001); x = sarw(x);
		case 13: c = sign(x, 0x0001); x = sarw(x);
		case 12: c = sign(x, 0x0001); x = sarw(x);
		case 11: c = sign(x, 0x0001); x = sarw(x);
		case 10: c = sign(x, 0x0001); x = sarw(x);
		case  9: c = sign(x, 0x0001); x = sarw(x);
		case  8: c = sign(x, 0x0001); x = sarw(x);
		case  7: c = sign(x, 0x0001); x = sarw(x);
		case  6: c = sign(x, 0x0001); x = sarw(x);
		case  5: c = sign(x, 0x0001); x = sarw(x);
		case  4: c = sign(x, 0x0001); x = sarw(x);
		case  3: c = sign(x, 0x0001); x = sarw(x);
		case  2: c = sign(x, 0x0001); x = sarw(x);
		case  1: c = sign(x, 0x0001); x = sarw(x);
		case  0: goto done;

		default:
			x = sign(x, 0x8000)? 0xffff: 0;

	}

done:
	if (v) {

		cpu->flags.c = c;
		cpu->flags.v = sign(x, 0x8000) ^ sign(x, 0x4000);

	}

	if (b > 0) {

		cpu->flags.p = parity(x);
		cpu->flags.a = false;
		cpu->flags.z = zero(x, 0x8000);
		cpu->flags.s = sign(x, 0x8000);

	}

	return x;

}


void op_addaib(CPU) { cpu->regs.ax.l = add8(cpu, cpu->regs.ax.l, (u8)cpu->insn.imm0); }
void op_ioraib(CPU) { cpu->regs.ax.l = ior8(cpu, cpu->regs.ax.l, (u8)cpu->insn.imm0); }
void op_adcaib(CPU) { cpu->regs.ax.l = adc8(cpu, cpu->regs.ax.l, (u8)cpu->insn.imm0); }
void op_sbbaib(CPU) { cpu->regs.ax.l = sbb8(cpu, cpu->regs.ax.l, (u8)cpu->insn.imm0); }
void op_andaib(CPU) { cpu->regs.ax.l = and8(cpu, cpu->regs.ax.l, (u8)cpu->insn.imm0); }
void op_subaib(CPU) { cpu->regs.ax.l = sub8(cpu, cpu->regs.ax.l, (u8)cpu->insn.imm0); }
void op_xoraib(CPU) { cpu->regs.ax.l = xor8(cpu, cpu->regs.ax.l, (u8)cpu->insn.imm0); }
void op_cmpaib(CPU) {                  sub8(cpu, cpu->regs.ax.l, (u8)cpu->insn.imm0); }

void op_addaiw(CPU) { cpu->regs.ax.w = add16(cpu, cpu->regs.ax.w, (u16)cpu->insn.imm0); }
void op_ioraiw(CPU) { cpu->regs.ax.w = ior16(cpu, cpu->regs.ax.w, (u16)cpu->insn.imm0); }
void op_adcaiw(CPU) { cpu->regs.ax.w = adc16(cpu, cpu->regs.ax.w, (u16)cpu->insn.imm0); }
void op_sbbaiw(CPU) { cpu->regs.ax.w = sbb16(cpu, cpu->regs.ax.w, (u16)cpu->insn.imm0); }
void op_andaiw(CPU) { cpu->regs.ax.w = and16(cpu, cpu->regs.ax.w, (u16)cpu->insn.imm0); }
void op_subaiw(CPU) { cpu->regs.ax.w = sub16(cpu, cpu->regs.ax.w, (u16)cpu->insn.imm0); }
void op_xoraiw(CPU) { cpu->regs.ax.w = xor16(cpu, cpu->regs.ax.w, (u16)cpu->insn.imm0); }
void op_cmpaiw(CPU) {                  sub16(cpu, cpu->regs.ax.w, (u16)cpu->insn.imm0); }

void op_addrmib(CPU) { aluu tmp = LDEAR1MB(); tmp = add8(cpu, tmp, (u8)cpu->insn.imm1); STEAR1MB(tmp); }
void op_iorrmib(CPU) { aluu tmp = LDEAR1MB(); tmp = ior8(cpu, tmp, (u8)cpu->insn.imm1); STEAR1MB(tmp); }
void op_adcrmib(CPU) { aluu tmp = LDEAR1MB(); tmp = adc8(cpu, tmp, (u8)cpu->insn.imm1); STEAR1MB(tmp); }
void op_sbbrmib(CPU) { aluu tmp = LDEAR1MB(); tmp = sbb8(cpu, tmp, (u8)cpu->insn.imm1); STEAR1MB(tmp); }
void op_andrmib(CPU) { aluu tmp = LDEAR1MB(); tmp = and8(cpu, tmp, (u8)cpu->insn.imm1); STEAR1MB(tmp); }
void op_subrmib(CPU) { aluu tmp = LDEAR1MB(); tmp = sub8(cpu, tmp, (u8)cpu->insn.imm1); STEAR1MB(tmp); }
void op_xorrmib(CPU) { aluu tmp = LDEAR1MB(); tmp = xor8(cpu, tmp, (u8)cpu->insn.imm1); STEAR1MB(tmp); }
void op_cmprmib(CPU) { aluu tmp = LDEAR1MB();       sub8(cpu, tmp, (u8)cpu->insn.imm1); }

void op_addrmiw(CPU) { aluu tmp = LDEAR1MW(); tmp = add16(cpu, tmp, (u16)cpu->insn.imm1); STEAR1MW(tmp); }
void op_iorrmiw(CPU) { aluu tmp = LDEAR1MW(); tmp = ior16(cpu, tmp, (u16)cpu->insn.imm1); STEAR1MW(tmp); }
void op_adcrmiw(CPU) { aluu tmp = LDEAR1MW(); tmp = adc16(cpu, tmp, (u16)cpu->insn.imm1); STEAR1MW(tmp); }
void op_sbbrmiw(CPU) { aluu tmp = LDEAR1MW(); tmp = sbb16(cpu, tmp, (u16)cpu->insn.imm1); STEAR1MW(tmp); }
void op_andrmiw(CPU) { aluu tmp = LDEAR1MW(); tmp = and16(cpu, tmp, (u16)cpu->insn.imm1); STEAR1MW(tmp); }
void op_subrmiw(CPU) { aluu tmp = LDEAR1MW(); tmp = sub16(cpu, tmp, (u16)cpu->insn.imm1); STEAR1MW(tmp); }
void op_xorrmiw(CPU) { aluu tmp = LDEAR1MW(); tmp = xor16(cpu, tmp, (u16)cpu->insn.imm1); STEAR1MW(tmp); }
void op_cmprmiw(CPU) { aluu tmp = LDEAR1MW();       sub16(cpu, tmp, (u16)cpu->insn.imm1); }

void op_addrmbf(CPU) { aluu tmp = LDEAR1MB(); *cpu->insn.reg0b = add8(cpu, *cpu->insn.reg0b, tmp); }
void op_iorrmbf(CPU) { aluu tmp = LDEAR1MB(); *cpu->insn.reg0b = ior8(cpu, *cpu->insn.reg0b, tmp); }
void op_adcrmbf(CPU) { aluu tmp = LDEAR1MB(); *cpu->insn.reg0b = adc8(cpu, *cpu->insn.reg0b, tmp); }
void op_sbbrmbf(CPU) { aluu tmp = LDEAR1MB(); *cpu->insn.reg0b = sbb8(cpu, *cpu->insn.reg0b, tmp); }
void op_andrmbf(CPU) { aluu tmp = LDEAR1MB(); *cpu->insn.reg0b = and8(cpu, *cpu->insn.reg0b, tmp); }
void op_subrmbf(CPU) { aluu tmp = LDEAR1MB(); *cpu->insn.reg0b = sub8(cpu, *cpu->insn.reg0b, tmp); }
void op_xorrmbf(CPU) { aluu tmp = LDEAR1MB(); *cpu->insn.reg0b = xor8(cpu, *cpu->insn.reg0b, tmp); }
void op_cmprmbf(CPU) { aluu tmp = LDEAR1MB();                    sub8(cpu, *cpu->insn.reg0b, tmp); }

void op_addrmwf(CPU) { aluu tmp = LDEAR1MW(); *cpu->insn.reg0w = add16(cpu, *cpu->insn.reg0w, tmp); }
void op_iorrmwf(CPU) { aluu tmp = LDEAR1MW(); *cpu->insn.reg0w = ior16(cpu, *cpu->insn.reg0w, tmp); }
void op_adcrmwf(CPU) { aluu tmp = LDEAR1MW(); *cpu->insn.reg0w = adc16(cpu, *cpu->insn.reg0w, tmp); }
void op_sbbrmwf(CPU) { aluu tmp = LDEAR1MW(); *cpu->insn.reg0w = sbb16(cpu, *cpu->insn.reg0w, tmp); }
void op_andrmwf(CPU) { aluu tmp = LDEAR1MW(); *cpu->insn.reg0w = and16(cpu, *cpu->insn.reg0w, tmp); }
void op_subrmwf(CPU) { aluu tmp = LDEAR1MW(); *cpu->insn.reg0w = sub16(cpu, *cpu->insn.reg0w, tmp); }
void op_xorrmwf(CPU) { aluu tmp = LDEAR1MW(); *cpu->insn.reg0w = xor16(cpu, *cpu->insn.reg0w, tmp); }
void op_cmprmwf(CPU) { aluu tmp = LDEAR1MW();                    sub16(cpu, *cpu->insn.reg0w, tmp); }

void op_addrmbr(CPU) { aluu tmp = LDEAR1MB(); tmp = add8(cpu, tmp, *cpu->insn.reg0b); STEAR1MB(tmp); }
void op_iorrmbr(CPU) { aluu tmp = LDEAR1MB(); tmp = ior8(cpu, tmp, *cpu->insn.reg0b); STEAR1MB(tmp); }
void op_adcrmbr(CPU) { aluu tmp = LDEAR1MB(); tmp = adc8(cpu, tmp, *cpu->insn.reg0b); STEAR1MB(tmp); }
void op_sbbrmbr(CPU) { aluu tmp = LDEAR1MB(); tmp = sbb8(cpu, tmp, *cpu->insn.reg0b); STEAR1MB(tmp); }
void op_andrmbr(CPU) { aluu tmp = LDEAR1MB(); tmp = and8(cpu, tmp, *cpu->insn.reg0b); STEAR1MB(tmp); }
void op_subrmbr(CPU) { aluu tmp = LDEAR1MB(); tmp = sub8(cpu, tmp, *cpu->insn.reg0b); STEAR1MB(tmp); }
void op_xorrmbr(CPU) { aluu tmp = LDEAR1MB(); tmp = xor8(cpu, tmp, *cpu->insn.reg0b); STEAR1MB(tmp); }
void op_cmprmbr(CPU) { aluu tmp = LDEAR1MB();       sub8(cpu, tmp, *cpu->insn.reg0b); }

void op_addrmwr(CPU) { aluu tmp = LDEAR1MW(); tmp = add16(cpu, tmp, *cpu->insn.reg0w); STEAR1MW(tmp); }
void op_iorrmwr(CPU) { aluu tmp = LDEAR1MW(); tmp = ior16(cpu, tmp, *cpu->insn.reg0w); STEAR1MW(tmp); }
void op_adcrmwr(CPU) { aluu tmp = LDEAR1MW(); tmp = adc16(cpu, tmp, *cpu->insn.reg0w); STEAR1MW(tmp); }
void op_sbbrmwr(CPU) { aluu tmp = LDEAR1MW(); tmp = sbb16(cpu, tmp, *cpu->insn.reg0w); STEAR1MW(tmp); }
void op_andrmwr(CPU) { aluu tmp = LDEAR1MW(); tmp = and16(cpu, tmp, *cpu->insn.reg0w); STEAR1MW(tmp); }
void op_subrmwr(CPU) { aluu tmp = LDEAR1MW(); tmp = sub16(cpu, tmp, *cpu->insn.reg0w); STEAR1MW(tmp); }
void op_xorrmwr(CPU) { aluu tmp = LDEAR1MW(); tmp = xor16(cpu, tmp, *cpu->insn.reg0w); STEAR1MW(tmp); }
void op_cmprmwr(CPU) { aluu tmp = LDEAR1MW();       sub16(cpu, tmp, *cpu->insn.reg0w); }

void op_decrmb(CPU) { aluu tmp = LDEAR1MB(); tmp = dec8(cpu, tmp); STEAR1MB(tmp); }
void op_incrmb(CPU) { aluu tmp = LDEAR1MB(); tmp = inc8(cpu, tmp); STEAR1MB(tmp); }

void op_decrmw(CPU) { aluu tmp = LDEAR1MW(); tmp = dec16(cpu, tmp); STEAR1MW(tmp); }
void op_incrmw(CPU) { aluu tmp = LDEAR1MW(); tmp = inc16(cpu, tmp); STEAR1MW(tmp); }

void op_decrw(CPU) { *cpu->insn.reg0w = dec16(cpu, *cpu->insn.reg0w); }
void op_incrw(CPU) { *cpu->insn.reg0w = inc16(cpu, *cpu->insn.reg0w); }

void op_rolb1(CPU) { aluu tmp = LDEAR1MB(); tmp = rol8(cpu, tmp, 1, true); STEAR1MB(tmp); }
void op_rorb1(CPU) { aluu tmp = LDEAR1MB(); tmp = ror8(cpu, tmp, 1, true); STEAR1MB(tmp); }
void op_rclb1(CPU) { aluu tmp = LDEAR1MB(); tmp = rcl8(cpu, tmp, 1, true); STEAR1MB(tmp); }
void op_rcrb1(CPU) { aluu tmp = LDEAR1MB(); tmp = rcr8(cpu, tmp, 1, true); STEAR1MB(tmp); }
void op_shlb1(CPU) { aluu tmp = LDEAR1MB(); tmp = shl8(cpu, tmp, 1, true); STEAR1MB(tmp); }
void op_shrb1(CPU) { aluu tmp = LDEAR1MB(); tmp = shr8(cpu, tmp, 1, true); STEAR1MB(tmp); }
void op_salb1(CPU) { aluu tmp = LDEAR1MB(); tmp = sal8(cpu, tmp, 1, true); STEAR1MB(tmp); }
void op_sarb1(CPU) { aluu tmp = LDEAR1MB(); tmp = sar8(cpu, tmp, 1, true); STEAR1MB(tmp); }

void op_rolw1(CPU) { aluu tmp = LDEAR1MW(); tmp = rol16(cpu, tmp, 1, true); STEAR1MW(tmp); }
void op_rorw1(CPU) { aluu tmp = LDEAR1MW(); tmp = ror16(cpu, tmp, 1, true); STEAR1MW(tmp); }
void op_rclw1(CPU) { aluu tmp = LDEAR1MW(); tmp = rcl16(cpu, tmp, 1, true); STEAR1MW(tmp); }
void op_rcrw1(CPU) { aluu tmp = LDEAR1MW(); tmp = rcr16(cpu, tmp, 1, true); STEAR1MW(tmp); }
void op_shlw1(CPU) { aluu tmp = LDEAR1MW(); tmp = shl16(cpu, tmp, 1, true); STEAR1MW(tmp); }
void op_shrw1(CPU) { aluu tmp = LDEAR1MW(); tmp = shr16(cpu, tmp, 1, true); STEAR1MW(tmp); }
void op_salw1(CPU) { aluu tmp = LDEAR1MW(); tmp = sal16(cpu, tmp, 1, true); STEAR1MW(tmp); }
void op_sarw1(CPU) { aluu tmp = LDEAR1MW(); tmp = sar16(cpu, tmp, 1, true); STEAR1MW(tmp); }

void op_rolbr(CPU) { aluu tmp = LDEAR1MB(); tmp = rol8(cpu, tmp, cpu->regs.cx.l, false); STEAR1MB(tmp); }
void op_rorbr(CPU) { aluu tmp = LDEAR1MB(); tmp = ror8(cpu, tmp, cpu->regs.cx.l, false); STEAR1MB(tmp); }
void op_rclbr(CPU) { aluu tmp = LDEAR1MB(); tmp = rcl8(cpu, tmp, cpu->regs.cx.l, false); STEAR1MB(tmp); }
void op_rcrbr(CPU) { aluu tmp = LDEAR1MB(); tmp = rcr8(cpu, tmp, cpu->regs.cx.l, false); STEAR1MB(tmp); }
void op_shlbr(CPU) { aluu tmp = LDEAR1MB(); tmp = shl8(cpu, tmp, cpu->regs.cx.l, false); STEAR1MB(tmp); }
void op_shrbr(CPU) { aluu tmp = LDEAR1MB(); tmp = shr8(cpu, tmp, cpu->regs.cx.l, false); STEAR1MB(tmp); }
void op_salbr(CPU) { aluu tmp = LDEAR1MB(); tmp = sal8(cpu, tmp, cpu->regs.cx.l, false); STEAR1MB(tmp); }
void op_sarbr(CPU) { aluu tmp = LDEAR1MB(); tmp = sar8(cpu, tmp, cpu->regs.cx.l, false); STEAR1MB(tmp); }

void op_rolwr(CPU) { aluu tmp = LDEAR1MW(); tmp = rol16(cpu, tmp, cpu->regs.cx.l, false); STEAR1MW(tmp); }
void op_rorwr(CPU) { aluu tmp = LDEAR1MW(); tmp = ror16(cpu, tmp, cpu->regs.cx.l, false); STEAR1MW(tmp); }
void op_rclwr(CPU) { aluu tmp = LDEAR1MW(); tmp = rcl16(cpu, tmp, cpu->regs.cx.l, false); STEAR1MW(tmp); }
void op_rcrwr(CPU) { aluu tmp = LDEAR1MW(); tmp = rcr16(cpu, tmp, cpu->regs.cx.l, false); STEAR1MW(tmp); }
void op_shlwr(CPU) { aluu tmp = LDEAR1MW(); tmp = shl16(cpu, tmp, cpu->regs.cx.l, false); STEAR1MW(tmp); }
void op_shrwr(CPU) { aluu tmp = LDEAR1MW(); tmp = shr16(cpu, tmp, cpu->regs.cx.l, false); STEAR1MW(tmp); }
void op_salwr(CPU) { aluu tmp = LDEAR1MW(); tmp = sal16(cpu, tmp, cpu->regs.cx.l, false); STEAR1MW(tmp); }
void op_sarwr(CPU) { aluu tmp = LDEAR1MW(); tmp = sar16(cpu, tmp, cpu->regs.cx.l, false); STEAR1MW(tmp); }

void op_movambf(CPU) { cpu->regs.ax.l = bus_read8( &cpu->mem, cpu->insn.addr); }
void op_movamwf(CPU) { cpu->regs.ax.w = bus_read16(&cpu->mem, cpu->insn.addr); }

void op_movambr(CPU) { bus_write8( &cpu->mem, cpu->insn.addr, cpu->regs.ax.l); }
void op_movamwr(CPU) { bus_write16(&cpu->mem, cpu->insn.addr, cpu->regs.ax.w); }

void op_movrib(CPU)  { *cpu->insn.reg0b = cpu->insn.imm0; }
void op_movriw(CPU)  { *cpu->insn.reg0w = cpu->insn.imm0; }

void op_movrmib(CPU) { STEAR1MB(cpu->insn.imm1); }
void op_movrmiw(CPU) { STEAR1MW(cpu->insn.imm1); }

void op_movrrmbf(CPU) { *cpu->insn.reg0b = LDEAR1MB(); }
void op_movrrmbr(CPU) { STEAR1MB(*cpu->insn.reg0b);}

void op_movrrmwf(CPU) { *cpu->insn.reg0w = LDEAR1MW(); }
void op_movrrmwr(CPU) { STEAR1MW(*cpu->insn.reg0w); }



static void op_divrmb(CPU);
static void op_divrmw(CPU);

static void op_idivrmb(CPU);
static void op_idivrmw(CPU);

static void op_imulrmb(CPU);
static void op_imulrmw(CPU);

static void op_mulrmb(CPU);
static void op_mulrmw(CPU);

static void op_negrmb(CPU);
static void op_negrmw(CPU);

static void op_notrmb(CPU);
static void op_notrmw(CPU);

static void op_tstaib(CPU);
static void op_tstaiw(CPU);
static void op_tstrib(CPU);
static void op_tstriw(CPU);
static void op_tstrmb(CPU);
static void op_tstrmw(CPU);


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
static void op_sahf(CPU);
static void op_xcharw(CPU);
static void op_xchrmb(CPU);
static void op_xchrmw(CPU);
static void op_xlatab(CPU);

static void op_popcs( CPU);   // Stack
static void op_popds( CPU);
static void op_popes( CPU);
static void op_popss( CPU);
static void op_popfw( CPU);
static void op_poprmw(CPU);
static void op_poprw( CPU);
static void op_pushcs(CPU);
static void op_pushds(CPU);
static void op_pushes(CPU);
static void op_pushss(CPU);
static void op_pushsp(CPU);
static void op_pushfw(CPU);
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

static void op_repne(CPU);
static void op_repeq(CPU);

static void op_lock(CPU);
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

static void op_undef(CPU);   // Invalid
static void op_group(CPU);
static void op_nop(CPU);


typedef void (*opcode)(CPU);

enum {

	OP_MODRM  = 1 << 8,  // Requires MODRM
	OP_W8     = 0 << 9,  // Is 8-bit operation
	OP_W16    = 1 << 9,  // Is 16-bit operation
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

	OP_RMB   = OP_MODRM | OP_W8,
	OP_RMW   = OP_MODRM | OP_W16,
	OP_R02B  = OP_R02   | OP_W8,
	OP_R02W  = OP_R02   | OP_W16,
	OP_ADRF  = OP_I0W   | OP_I1W,
	OP_ADRN  = OP_I0W,
	OP_ADRR  = OP_I0B,

	OP_RIBB = OP_I0B | OP_W8,
	OP_RIWB = OP_I0W | OP_W8,

	OP_RIBW = OP_I0B | OP_W16,
	OP_RIWW = OP_I0W | OP_W16,

	OP_RMIB = OP_MODRM | OP_I1B | OP_W8,
	OP_RMIW = OP_MODRM | OP_I1W | OP_W16,

	OP_R2IB = OP_R02   | OP_W8  | OP_I0B,
	OP_R2IW = OP_R02   | OP_W16 | OP_I0W,
	OP_SMB  = OP_MODRM | OP_W16 | OP_RSEG,

	OP_RANB = OP_EASEG | OP_EAIMM0 | OP_I0W | OP_W8,
	OP_RANW = OP_EASEG | OP_EAIMM0 | OP_I0W | OP_W16,

	OP_G0  =  0 | OP_RMB | OP_GROUP | OP_I1B,
	OP_G1  =  1 | OP_RMW | OP_GROUP | OP_I1W,
	OP_G2  =  2 | OP_RMB | OP_GROUP | OP_I1B,
	OP_G3  =  3 | OP_RMW | OP_GROUP | OP_I1B,
	OP_G4  =  4 | OP_RMB | OP_GROUP,
	OP_G5  =  5 | OP_RMW | OP_GROUP,
	OP_G6  =  6 | OP_RMB | OP_GROUP,
	OP_G7  =  7 | OP_RMW | OP_GROUP,
	OP_G8  =  8 | OP_RMB | OP_GROUP,
	OP_G9  =  9 | OP_RMW | OP_GROUP,
	OP_G10 = 10 | OP_RMB | OP_GROUP,
	OP_G11 = 11 | OP_RMW | OP_GROUP

};


static const opcode opcodes[352] = {

// Main opcodes
// 0x00
	&op_addrmbr,  &op_addrmwr,  &op_addrmbf,  &op_addrmwf,  &op_addaib,  &op_addaiw,  &op_pushes,  &op_popes,
	&op_iorrmbr,  &op_iorrmwr,  &op_iorrmbf,  &op_iorrmwf,  &op_ioraib,  &op_ioraiw,  &op_pushcs,  &op_popcs,
	&op_adcrmbr,  &op_adcrmwr,  &op_adcrmbf,  &op_adcrmwf,  &op_adcaib,  &op_adcaiw,  &op_pushss,  &op_popss,
	&op_sbbrmbr,  &op_sbbrmwr,  &op_sbbrmbf,  &op_sbbrmwf,  &op_sbbaib,  &op_sbbaiw,  &op_pushds,  &op_popds,
// 0x20
	&op_andrmbr,  &op_andrmwr,  &op_andrmbf,  &op_andrmwf,  &op_andaib,  &op_andaiw,  &op_seges,   &op_daa,
	&op_subrmbr,  &op_subrmwr,  &op_subrmbf,  &op_subrmwf,  &op_subaib,  &op_subaiw,  &op_segcs,   &op_das,
	&op_xorrmbr,  &op_xorrmwr,  &op_xorrmbf,  &op_xorrmwf,  &op_xoraib,  &op_xoraiw,  &op_segss,   &op_aaa,
	&op_cmprmbr,  &op_cmprmwr,  &op_cmprmbf,  &op_cmprmwf,  &op_cmpaib,  &op_cmpaiw,  &op_segds,   &op_aas,
// 0x40
	&op_incrw,   &op_incrw,   &op_incrw,   &op_incrw,   &op_incrw,   &op_incrw,   &op_incrw,   &op_incrw,
	&op_decrw,   &op_decrw,   &op_decrw,   &op_decrw,   &op_decrw,   &op_decrw,   &op_decrw,   &op_decrw,
	&op_pushrw,  &op_pushrw,  &op_pushrw,  &op_pushrw,  &op_pushsp,  &op_pushrw,  &op_pushrw,  &op_pushrw,
	&op_poprw,   &op_poprw,   &op_poprw,   &op_poprw,   &op_poprw,   &op_poprw,   &op_poprw,   &op_poprw,
// 0x60
	&op_jco,     &op_jcno,    &op_jcc,     &op_jcnc,    &op_jcz,     &op_jcnz,    &op_jcbe,    &op_jcnbe,
	&op_jcs,     &op_jcns,    &op_jcp,     &op_jcnp,    &op_jcl,     &op_jcnl,    &op_jcle,    &op_jcnle,
	&op_jco,     &op_jcno,    &op_jcc,     &op_jcnc,    &op_jcz,     &op_jcnz,    &op_jcbe,    &op_jcnbe,
	&op_jcs,     &op_jcns,    &op_jcp,     &op_jcnp,    &op_jcl,     &op_jcnl,    &op_jcle,    &op_jcnle,
// 0x80
	&op_group,    &op_group,    &op_group,    &op_group,    &op_tstrmb,  &op_tstrmw, &op_xchrmb,  &op_xchrmw,
	&op_movrrmbr, &op_movrrmwr, &op_movrrmbf, &op_movrrmwf, &op_movrrmwr, &op_lear,   &op_movrrmwf, &op_poprmw,
	&op_xcharw,   &op_xcharw,   &op_xcharw,   &op_xcharw,   &op_xcharw,  &op_xcharw, &op_xcharw,  &op_xcharw,
	&op_cbw,      &op_cwd,      &op_callf,    &op_wait,     &op_pushfw,   &op_popfw,   &op_sahf,    &op_lahf,
// 0xa0
	&op_movambf, &op_movamwf, &op_movambr, &op_movamwr, &op_movsb,   &op_movsw,   &op_cmpsb,   &op_cmpsw,
	&op_tstaib,  &op_tstaiw,  &op_stosb,   &op_stosw,   &op_lodsb,   &op_lodsw,   &op_scasb,   &op_scasw,
	&op_movrib,  &op_movrib,  &op_movrib,  &op_movrib,  &op_movrib,  &op_movrib,  &op_movrib,  &op_movrib,
	&op_movriw,  &op_movriw,  &op_movriw,  &op_movriw,  &op_movriw,  &op_movriw,  &op_movriw,  &op_movriw,
// 0xc0
	&op_retn,    &op_retn,    &op_retn,    &op_retn,    &op_lesr,    &op_ldsr,    &op_movrmib, &op_movrmiw,
	&op_retf,    &op_retf,    &op_retf,    &op_retf,    &op_int3,    &op_intib,   &op_into,    &op_iret,
	&op_group,   &op_group,   &op_group,   &op_group,   &op_aam,     &op_aad,     &op_undef,   &op_xlatab,
	&op_nop,     &op_nop,     &op_nop,     &op_nop,     &op_nop,     &op_nop,     &op_nop,     &op_nop,
// 0xe0
	&op_loopnzr, &op_loopzr,  &op_loopr,   &op_jcxzr,   &op_inib,    &op_iniw,    &op_outib,   &op_outiw,
	&op_calln,   &op_jmpn,    &op_jmpf,    &op_jmpn,    &op_inrb,    &op_inrw,    &op_outrb,   &op_outrw,
	&op_lock,    &op_undef,   &op_repne,   &op_repeq,    &op_nop,     &op_cmc,     &op_group,   &op_group,
	&op_clc,     &op_stc,     &op_cli,     &op_sti,     &op_cld,     &op_std,     &op_group,   &op_group,

// Group opcodes
// 0x100
	&op_addrmib, &op_iorrmib, &op_adcrmib, &op_sbbrmib, &op_andrmib, &op_subrmib, &op_xorrmib, &op_cmprmib,  // 0:  0x80
	&op_addrmiw, &op_iorrmiw, &op_adcrmiw, &op_sbbrmiw, &op_andrmiw, &op_subrmiw, &op_xorrmiw, &op_cmprmiw,  // 1:  0x81
	&op_addrmib, &op_iorrmib, &op_adcrmib, &op_sbbrmib, &op_andrmib, &op_subrmib, &op_xorrmib, &op_cmprmib,  // 2:  0x82
	&op_addrmiw, &op_iorrmiw, &op_adcrmiw, &op_sbbrmiw, &op_andrmiw, &op_subrmiw, &op_xorrmiw, &op_cmprmiw,  // 3:  0x83
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
	OP_RMB,  OP_RMW,  OP_RMB,  OP_RMW,  OP_RIBB, OP_RIWW, 0,        0,
	OP_RMB,  OP_RMW,  OP_RMB,  OP_RMW,  OP_RIBB, OP_RIWW, 0,        0,
	OP_RMB,  OP_RMW,  OP_RMB,  OP_RMW,  OP_RIBB, OP_RIWW, 0,        0,
	OP_RMB,  OP_RMW,  OP_RMB,  OP_RMW,  OP_RIBB, OP_RIWW, 0,        0,
// 0x20
	OP_RMB,  OP_RMW,  OP_RMB,  OP_RMW,  OP_RIBB, OP_RIWW, OP_OVRD,  0,
	OP_RMB,  OP_RMW,  OP_RMB,  OP_RMW,  OP_RIBB, OP_RIWW, OP_OVRD,  0,
	OP_RMB,  OP_RMW,  OP_RMB,  OP_RMW,  OP_RIBB, OP_RIWW, OP_OVRD,  0,
	OP_RMB,  OP_RMW,  OP_RMB,  OP_RMW,  OP_RIBB, OP_RIWW, OP_OVRD,  0,
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
	OP_G0,    OP_G1,    OP_G2,    OP_G3,    OP_RMB,   OP_RMW,  OP_RMB,  OP_RMW,
	OP_RMB,   OP_RMW,   OP_RMB,   OP_RMW,   OP_SMB,   OP_RMW | OP_NEASEG,  OP_SMB,  OP_RMW,
	OP_R02W,  OP_R02W,  OP_R02W,  OP_R02W,  OP_R02W,  OP_R02W,  OP_R02W,  OP_R02W,
	0,        0,        OP_ADRF,  0,        0,        0,        0,        0,
// 0xa0
	OP_RANB, OP_RANW,   OP_RANB,  OP_RANW,  OP_W8,    OP_W16,   OP_W8,    OP_W16,
	OP_RIBB, OP_RIWW,   OP_W8,    OP_W16,   OP_W8,    OP_W16,   0,        0,
	OP_R2IB,  OP_R2IB,  OP_R2IB,  OP_R2IB,  OP_R2IB,  OP_R2IB,  OP_R2IB,  OP_R2IB,
	OP_R2IW,  OP_R2IW,  OP_R2IW,  OP_R2IW,  OP_R2IW,  OP_R2IW,  OP_R2IW,  OP_R2IW,
// 0xc0
	OP_I0W,   0,        OP_I0W,   0,        OP_RMW,   OP_RMW,   OP_RMIB,  OP_RMIW,
	OP_I0W,   0,        OP_I0W,   0,        0,        OP_I0B,   0,        0,
	OP_G4,    OP_G5,    OP_G6,    OP_G7,    OP_RIBB,  OP_RIBB, 0,        0,
	OP_MODRM, OP_MODRM, OP_MODRM, OP_MODRM, OP_MODRM, OP_MODRM, OP_MODRM, OP_MODRM,
// 0xe0
	OP_ADRR,  OP_ADRR,  OP_ADRR,  OP_ADRR,  OP_RIBB, OP_RIBW, OP_RIBB, OP_RIBW,
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


static inline aluu memrdb(CPU, uint seg, u16 ofs) {
	return bus_read8(&cpu->mem, *get_rs(cpu, seg) * 16 + ofs);
}

static inline aluu memrdw(CPU, uint seg, u16 ofs) {
	return bus_read16(&cpu->mem, *get_rs(cpu, seg) * 16 + ofs);
}

static inline void memwrb(CPU, uint seg, u16 ofs, aluu v) {
	bus_write8(&cpu->mem, *get_rs(cpu, seg) * 16 + ofs, v);
}

static inline void memwrw(CPU, uint seg, u16 ofs, aluu v) {
	bus_write16(&cpu->mem, *get_rs(cpu, seg) * 16 + ofs, v);
}






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

	ADVSP(-6);
	STSPW(4, cpu->flags.w);
	STSPW(2, cpu->regs.cs);
	STSPW(0, cpu->regs.ip);

	cpu->flags.i = false;
	cpu->flags.t = false;

	cpu->regs.ip = bus_read16(&cpu->mem, 4 * (u8)irq + 0);
	cpu->regs.cs = bus_read16(&cpu->mem, 4 * (u8)irq + 2);

}



void i8086_tick(CPU)
{

	if (cpu->interrupt.nmi_act) {

		i8086_interrupt(cpu, I8086_VECTOR_NMI);

		cpu->interrupt.nmi_act = false;
		cpu->insn.fetch_opcode = true;

	} else if (cpu->interrupt.irq_act) {

		cpu->regs.cs = cpu->regs.scs;
		cpu->regs.ip = cpu->regs.sip;

		i8086_interrupt(cpu, cpu->interrupt.irq);

		cpu->interrupt.irq_act = false;
		cpu->insn.fetch_opcode = true;

	} else if (cpu->flags.t && cpu->insn.fetch_opcode)
		i8086_interrupt(cpu, I8086_VECTOR_SSTEP);


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



void op_divrmb(CPU)
{

	const aluu tmp = LDEAR1MB();

	if (tmp == 0)
		return i8086_interrupt(cpu, I8086_VECTOR_DIVERR);

	const aluu div = cpu->regs.ax.w / (u8)tmp;
	const aluu mod = cpu->regs.ax.w % (u8)tmp;

	if (div > 255)
		return i8086_interrupt(cpu, I8086_VECTOR_DIVERR);

	cpu->regs.ax.l = div;
	cpu->regs.ax.h = mod;

}



void op_divrmw(CPU)
{

	const aluu tmp = LDEAR1MW();

	if (tmp == 0)
		return i8086_interrupt(cpu, I8086_VECTOR_DIVERR);

	const aluu num = cpu->regs.dx.w * 65536 + cpu->regs.ax.w;
	const aluu div = num / (u16)tmp;
	const aluu mod = num % (u16)tmp;

	if (div > 65535)
		return i8086_interrupt(cpu, I8086_VECTOR_DIVERR);

	cpu->regs.ax.w = div;
	cpu->regs.dx.w = mod;

}



void op_idivrmb(CPU)
{

	const i8 tmp = (i8)LDEAR1MB();

	if (tmp == 0)
		return i8086_interrupt(cpu, I8086_VECTOR_DIVERR);

	const i16 num = (i16)cpu->regs.ax.w;
	const i16 div = num / tmp;
	const i16 mod = num % tmp;

	if (div <= -128 || div > 127)
		return i8086_interrupt(cpu, I8086_VECTOR_DIVERR);

	cpu->regs.ax.l = div;
	cpu->regs.ax.h = mod;

}



void op_idivrmw(CPU)
{

	const i16 tmp = (i16)LDEAR1MW();

	if (tmp == 0)
		return i8086_interrupt(cpu, I8086_VECTOR_DIVERR);

	const i32 num = (i32)((u32)(cpu->regs.dx.w << 16) + cpu->regs.ax.w);
	const i32 div = num / tmp;
	const i32 mod = num % tmp;

	if (div <= -32768 || div > 32767)
		return i8086_interrupt(cpu, I8086_VECTOR_DIVERR);

	cpu->regs.ax.w = div;
	cpu->regs.dx.w = mod;

}



void op_imulrmb(CPU)
{

	aluu tmp = (i8)LDEAR1MB();

	tmp *= (i8)cpu->regs.ax.l;

	cpu->regs.ax.w = tmp;

	cpu->flags.c = (i8)cpu->regs.ax.l != tmp;
	cpu->flags.v = cpu->flags.c;

}



void op_imulrmw(CPU)
{

	aluu tmp = (i16)LDEAR1MW();

	tmp *= (i16)cpu->regs.ax.w;

	cpu->regs.ax.w = (tmp >> 0)  & 0xffff;
	cpu->regs.dx.w = (tmp >> 16) & 0xffff;

	cpu->flags.c = (i16)cpu->regs.ax.w != tmp;
	cpu->flags.v = cpu->flags.c;

}


void op_mulrmb(CPU)
{

	aluu tmp = LDEAR1MB();

	cpu->regs.ax.w = cpu->regs.ax.l * tmp;

	cpu->flags.c = (cpu->regs.ax.h != 0);
	cpu->flags.v = cpu->flags.c;

}



void op_mulrmw(CPU)
{

	aluu tmp = LDEAR1MW();

	tmp *= cpu->regs.ax.w;

	cpu->regs.ax.w = (tmp >> 0)  & 0xffff;
	cpu->regs.dx.w = (tmp >> 16) & 0xffff;

	cpu->flags.c = (cpu->regs.dx.w != 0);
	cpu->flags.v = cpu->flags.c;

}



void op_negrmb(CPU) { aluu tmp = LDEAR1MB(); tmp = sub8( cpu, 0, tmp); STEAR1MB(tmp); }
void op_negrmw(CPU) { aluu tmp = LDEAR1MW(); tmp = sub16(cpu, 0, tmp); STEAR1MW(tmp); }


void op_tstaib(CPU) {                  and8(cpu, cpu->regs.ax.l, (u8)cpu->insn.imm0); }
void op_tstaiw(CPU) {                  and16(cpu, cpu->regs.ax.w, (u16)cpu->insn.imm0); }

void op_notrmb(CPU) { aluu tmp = LDEAR1MB(); tmp = ~tmp; STEAR1MB(tmp); }
void op_notrmw(CPU) { aluu tmp = LDEAR1MW(); tmp = ~tmp; STEAR1MW(tmp); }

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







void op_aaa(CPU)
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



void op_aas(CPU)
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



void op_aad(CPU)
{

        cpu->regs.ax.l = cpu->regs.ax.l + cpu->regs.ax.h * cpu->insn.imm0;
	cpu->regs.ax.h = 0;

	cpu->flags.p = parity(cpu->regs.ax.l);
	cpu->flags.z = zero(cpu->regs.ax.l, 0x80);
	cpu->flags.s = sign(cpu->regs.ax.l, 0x80);

}



void op_aam(CPU)
{

	if (cpu->insn.imm0 == 0) {

		cpu->flags.c = false;
		cpu->flags.v = false;
		cpu->flags.p = true;
		cpu->flags.a = false;
		cpu->flags.z = true;
		cpu->flags.s = false;

		return i8086_interrupt(cpu, I8086_VECTOR_DIVERR);

	}

	const aluu tmp = cpu->regs.ax.l;

	cpu->regs.ax.l = tmp % (u8)cpu->insn.imm0;
	cpu->regs.ax.h = tmp / (u8)cpu->insn.imm0;

	cpu->flags.p = parity(cpu->regs.ax.l);
	cpu->flags.z = zero(cpu->regs.ax.l, 0x80);
	cpu->flags.s = sign(cpu->regs.ax.l, 0x80);

}



void op_cbw(CPU) { cpu->regs.ax.w = (i8)cpu->regs.ax.l; }
void op_cwd(CPU) { cpu->regs.dx.w = sign(cpu->regs.ax.w, 0x8000)? 0xffff: 0x0000; }



void op_daa(CPU)
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

	cpu->flags.p = parity(cpu->regs.ax.l);
	cpu->flags.z = zero(cpu->regs.ax.l, 0x80);
	cpu->flags.s = sign(cpu->regs.ax.l, 0x80);

}



void op_das(CPU)
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

	cpu->flags.p = parity(cpu->regs.ax.l);
	cpu->flags.z = zero(cpu->regs.ax.l, 0x80);
	cpu->flags.s = sign(cpu->regs.ax.l, 0x80);

}



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



void op_popcs( CPU) { ADVSP(+2); cpu->regs.cs     = LDSPW(-2);  }
void op_popds( CPU) { ADVSP(+2); cpu->regs.ds     = LDSPW(-2);  }
void op_popes( CPU) { ADVSP(+2); cpu->regs.es     = LDSPW(-2);  }
void op_popss( CPU) { ADVSP(+2); cpu->regs.ss     = LDSPW(-2);  }
void op_popfw( CPU) { ADVSP(+2); cpu->flags.w     = (LDSPW(-2) & 0x0ed5) | 0xf002; }
void op_poprw( CPU) { ADVSP(+2); *cpu->insn.reg0w = LDSPW(-2); }
void op_poprmw(CPU) { ADVSP(+2); aluu tmp = LDSPW(-2); STEAR1MW(tmp); }

void op_pushcs(CPU)  { ADVSP(-2); STSPW(0, cpu->regs.cs); }
void op_pushds(CPU)  { ADVSP(-2); STSPW(0, cpu->regs.ds); }
void op_pushes(CPU)  { ADVSP(-2); STSPW(0, cpu->regs.es); }
void op_pushss(CPU)  { ADVSP(-2); STSPW(0, cpu->regs.ss); }
void op_pushsp(CPU)  { ADVSP(-2); STSPW(0, cpu->regs.sp.w); }
void op_pushfw(CPU)  { ADVSP(-2); STSPW(0, cpu->flags.w | 0x0002); }
void op_pushrw(CPU)  { ADVSP(-2); STSPW(0, *cpu->insn.reg0w); }
void op_pushrmw(CPU) { ADVSP(-2); aluu tmp = LDEAR1MW(); STSPW(0, tmp); }

void op_callf(CPU) { ADVSP(-4); STSPW(2, cpu->regs.cs); STSPW(0, cpu->regs.ip); cpu->regs.ip  = cpu->insn.imm0; cpu->regs.cs = cpu->insn.imm1; }
void op_calln(CPU) { ADVSP(-2); STSPW(0, cpu->regs.ip);                         cpu->regs.ip += cpu->insn.imm0; }



void op_callfrm(CPU)
{

	if (!cpu->insn.op_memory)
		op_undef(cpu);

	aluu tip = bus_read16(&cpu->mem, cpu->insn.addr + 0);
	aluu tcs = bus_read16(&cpu->mem, cpu->insn.addr + 2);

	ADVSP(-4);
	STSPW(2, cpu->regs.cs);
	STSPW(0, cpu->regs.ip);

	cpu->regs.ip = tip;
	cpu->regs.cs = tcs;

}



void op_callnrm(CPU) { aluu tmp = LDEAR1MW(); ADVSP(-2); STSPW(0, cpu->regs.ip); cpu->regs.ip = tmp; }

void op_int3(CPU)  { i8086_interrupt(cpu, I8086_VECTOR_BREAK); }
void op_into(CPU)  { if (cpu->flags.v) i8086_interrupt(cpu, I8086_VECTOR_VFLOW); }
void op_intib(CPU) { i8086_interrupt(cpu, cpu->insn.imm0); }

void op_iret(CPU) { ADVSP(+4); cpu->regs.ip = LDSPW(-4); cpu->regs.cs = LDSPW(-2); op_popfw(cpu); }

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

void op_jmpf(CPU) { cpu->regs.ip  = cpu->insn.imm0; cpu->regs.cs = cpu->insn.imm1; }
void op_jmpn(CPU) { cpu->regs.ip += cpu->insn.imm0; }

void op_jmpfrm(CPU)
{

	if (!cpu->insn.op_memory)
		op_undef(cpu);

	cpu->regs.ip = bus_read16(&cpu->mem, cpu->insn.addr + 0);
	cpu->regs.cs = bus_read16(&cpu->mem, cpu->insn.addr + 2);

}

void op_jmpnrm(CPU) { cpu->regs.ip = LDEAR1MW(); }

void op_loopnzr(CPU) { if (--cpu->regs.cx.w && !cpu->flags.z) cpu->regs.ip += cpu->insn.imm0; }
void op_loopzr(CPU)  { if (--cpu->regs.cx.w && cpu->flags.z)  cpu->regs.ip += cpu->insn.imm0; }
void op_loopr(CPU)   { if (--cpu->regs.cx.w)                  cpu->regs.ip += cpu->insn.imm0; }

void op_retf(CPU) { ADVSP(+4); cpu->regs.ip = LDSPW(-4); cpu->regs.cs = LDSPW(-2); ADVSP(cpu->insn.imm0); }
void op_retn(CPU) { ADVSP(+2); cpu->regs.ip = LDSPW(-2);                           ADVSP(cpu->insn.imm0); }

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
void op_repne(CPU) { cpu->insn.repeat_eq = false; cpu->insn.repeat_ne = true;  }
void op_repeq(CPU) { cpu->insn.repeat_eq = true;  cpu->insn.repeat_ne = false; }

void op_lock(CPU)  { /* FIXME: Not implemented */ }
void op_wait(CPU)  { /* FIXME: Not implemented */ }

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



void op_undef(CPU)
{

	i8086_undef(cpu);

}



void op_group(CPU) { }
void op_nop(CPU)   { }

