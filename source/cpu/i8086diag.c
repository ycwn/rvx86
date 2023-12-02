

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifdef HAVE_ZYDIS
#include <Zydis/Zydis.h>
#endif

#include "core/types.h"
#include "core/debug.h"
#include "core/io.h"
#include "core/wire.h"

#include "cpu/i8086.h"
#include "cpu/i8086diag.h"



void i8086_dump(struct i8086 *cpu)
{

	printf( "\n\n"
		"AX=%04x  BX=%04x  CX=%04x  DX=%04x  SP=%04x  BP=%04x  SI=%04x  DI=%04x\n"
		"DS=%04x  ES=%04x  SS=%04x  CS=%04x  IP=%04x   %s %s %s %s %s %s %s %s \n"
		"\n",

		cpu->regs.ax.w, cpu->regs.bx.w, cpu->regs.cx.w, cpu->regs.dx.w,
		cpu->regs.sp.w, cpu->regs.bp.w, cpu->regs.si.w, cpu->regs.di.w,
		cpu->regs.ds,   cpu->regs.es,   cpu->regs.ss,
		cpu->regs.cs,   cpu->regs.ip,

		cpu->flags.v? "OV": "NV",
		cpu->flags.d? "DN": "UP",
		cpu->flags.i? "EI": "DI",
		cpu->flags.s? "NG": "PL",
		cpu->flags.z? "ZR": "NZ",
		cpu->flags.a? "AC": "NA",
		cpu->flags.p? "PE": "PO",
		cpu->flags.c? "CY": "NC"
	);


#ifdef HAVE_ZYDIS

	u8  buf[16];
	u32 addr = cpu->regs.scs * 16 + cpu->regs.sip;

	for (int n=0; n < 16; n++)
		buf[n] = cpu->memory_base[(addr + n) & cpu->memory_mask];

	ZydisDisassembledInstruction insn;

	if (ZYAN_SUCCESS(ZydisDisassembleIntel(ZYDIS_MACHINE_MODE_REAL_16, addr, buf, sizeof(buf), &insn)))
		printf("%04x:%04x    %s\n\n", cpu->regs.scs, cpu->regs.sip, insn.text);

	else
		printf("%04x:%04x    ??\n\n", cpu->regs.scs, cpu->regs.sip);

#endif

}



void i8086_stack(struct i8086 *cpu)
{

	char buf[32];

	for (int n=8; n >= -8; n--) {

		u16  sp    = cpu->regs.sp.w + n * 2;
		uint addr  = cpu->regs.ss * 16 +  sp;
		uint value = *(u16*)(cpu->memory_base + ((addr + n) & cpu->memory_mask));

		if (cpu->regs.ss * 16 + cpu->regs.ss == addr)
			strcpy(buf, "BP +  0");

		else if (n < 0) sprintf(buf, "SP - %2d", -n * 2);
		else            sprintf(buf, "SP + %2d",  n * 2);

		printf("%04x:%04x: %05x    [%07s]    %04x\n", cpu->regs.ss, sp, addr, buf, value);

	}

}

