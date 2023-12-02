

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include <ctype.h>

#include "core/types.h"
#include "core/debug.h"
#include "core/bus.h"
#include "core/wire.h"

#include "cpu/i8086.h"

#include "util/fs.h"
#include "util/trim.h"

#include "device/ram.h"


#define COLOR_NONE       "\033[0m"
#define COLOR_BLACK      "\033[0;30m"
#define COLOR_RED        "\033[0;31m"
#define COLOR_GREEN      "\033[0;32m"
#define COLOR_BROWN      "\033[0;33m"
#define COLOR_BLUE       "\033[0;34m"
#define COLOR_MAGENTA    "\033[0;35m"
#define COLOR_CYAN       "\033[0;36m"
#define COLOR_LGRAY      "\033[0;37m"
#define COLOR_GRAY       "\033[1;30m"
#define COLOR_LRED       "\033[1;31m"
#define COLOR_LGREEN     "\033[1;32m"
#define COLOR_YELLOW     "\033[1;33m"
#define COLOR_LBLUE      "\033[1;34m"
#define COLOR_LMAGENTA   "\033[1;35m"
#define COLOR_LCYAN      "\033[1;36m"
#define COLOR_WHITE      "\033[1;37m"

#define COLOR(X, TEXT) COLOR_##X TEXT COLOR_NONE

#define TEXT_PASS  COLOR(LBLUE, "[") "" COLOR(LGREEN, "PASS") "" COLOR(LBLUE, "]") "   "
#define TEXT_FAIL  COLOR(LBLUE, "[") "" COLOR(LRED,   "FAIL") "" COLOR(LBLUE, "]") "   "
#define TEXT_WARN  COLOR(LBLUE, "[") "" COLOR(LCYAN,  "WARN") "" COLOR(LBLUE, "]") "   "
#define TEXT_UNDF  COLOR(LBLUE, "[") "" COLOR(YELLOW, "UNDF") "" COLOR(LBLUE, "]") "   "



struct test_report {

	char title[256];
	char name[256];

	bool failed;
	bool quiet;
	bool undef;

	int tests_passed;
	int tests_failed;

	int conditions_passed;
	int conditions_failed;

};


bool test_undefined = false;



void test_init(struct test_report *tr, const char *title)
{

	strcpy(tr->title, title);

	tr->failed = false;
	tr->quiet  = false;

	tr->tests_passed = 0;
	tr->tests_failed = 0;

	tr->conditions_passed = 0;
	tr->conditions_failed = 0;

}



void test_aggregate(struct test_report *tr, struct test_report *other)
{

	tr->tests_passed += other->tests_passed;
	tr->tests_failed += other->tests_failed;

	tr->conditions_passed += other->conditions_passed;
	tr->conditions_failed += other->conditions_failed;

}



void test_start(struct test_report *tr, const char *name)
{

	strcpy(tr->name, name);

	tr->failed     = false;
	test_undefined = false;

}



void test_complete(struct test_report *tr)
{

	if (test_undefined) {

		if (!tr->quiet)
			printf(TEXT_UNDF "%s\n", tr->name);

		tr->failed = true;

	}

	if (tr->failed) {

		tr->tests_failed++;

	} else {

		tr->tests_passed++;

		if (!tr->quiet)
			printf(TEXT_PASS "%s\n", tr->name);

	}

}



void i8086_undef(struct i8086 *cpu)
{

	test_undefined = true;

}



void test_expect(struct test_report *tr, const char *msg, uint val, uint exp)
{

	if (test_undefined)
		return;

	if (val != exp) {

		if (!tr->quiet && !tr->failed)
			printf(TEXT_FAIL "%s\n", tr->name);

		if (!tr->quiet)
			printf(TEXT_WARN "%s has value %#x, expected %#x\n", msg, val, exp);

		tr->conditions_failed++;
		tr->failed = true;

	}

	tr->conditions_passed++;

}



bool test_run_cases(struct test_report *tr, struct i8086 *cpu, const char *path)
{

	if (!fs_open(path, FS_RD)) {
		perror("test_run(): fs_open()");
		return false;
	}

	char *line;
	bool  first    = true;
	bool  executed = false;
	uint  undef    = 0xffff;


	while (line = fs_gets()) {

		*strchrnul(line, '#') = 0;

		line = trim(line);

		if (*line == 0)
			continue;

		if (line[0] == 'T') {

			if (!first)
				test_complete(tr);

			test_start(tr, &line[1]);

			first    = false;
			executed = false;
			undef    = 0xffff;

			i8086_reset(cpu);

		} else if (line[0] == 'U') {

			undef = strtoul(&line[1], NULL, 16);

		} else if (line[0] == 'R') {

			uint flags = strtoul(&line[ 1], NULL, 16);
			uint ax    = strtoul(&line[ 6], NULL, 16);
			uint bx    = strtoul(&line[11], NULL, 16);
			uint cx    = strtoul(&line[16], NULL, 16);
			uint dx    = strtoul(&line[21], NULL, 16);
			uint si    = strtoul(&line[26], NULL, 16);
			uint di    = strtoul(&line[31], NULL, 16);
			uint bp    = strtoul(&line[36], NULL, 16);
			uint sp    = strtoul(&line[41], NULL, 16);
			uint ip    = strtoul(&line[46], NULL, 16);
			uint cs    = strtoul(&line[51], NULL, 16);
			uint ds    = strtoul(&line[56], NULL, 16);
			uint es    = strtoul(&line[61], NULL, 16);
			uint ss    = strtoul(&line[66], NULL, 16);

			if (executed) {

				test_expect(tr, "FLAGS",       cpu->flags.w & undef, flags & undef);
				test_expect(tr, "Register AX", cpu->regs.ax.w, ax);
				test_expect(tr, "Register BX", cpu->regs.bx.w, bx);
				test_expect(tr, "Register CX", cpu->regs.cx.w, cx);
				test_expect(tr, "Register DX", cpu->regs.dx.w, dx);
				test_expect(tr, "Register SI", cpu->regs.si.w, si);
				test_expect(tr, "Register DI", cpu->regs.di.w, di);
				test_expect(tr, "Register BP", cpu->regs.bp.w, bp);
				test_expect(tr, "Register SP", cpu->regs.sp.w, sp);
				test_expect(tr, "Register IP", cpu->regs.ip,   ip);
				test_expect(tr, "Register CS", cpu->regs.cs,   cs);
				test_expect(tr, "Register DS", cpu->regs.ds,   ds);
				test_expect(tr, "Register ES", cpu->regs.es,   es);
				test_expect(tr, "Register SS", cpu->regs.ss,   ss);

			} else {

				cpu->flags.w   = flags;
				cpu->regs.ax.w = ax;
				cpu->regs.bx.w = bx;
				cpu->regs.cx.w = cx;
				cpu->regs.dx.w = dx;
				cpu->regs.si.w = si;
				cpu->regs.di.w = di;
				cpu->regs.bp.w = bp;
				cpu->regs.sp.w = sp;
				cpu->regs.ip   = ip;
				cpu->regs.cs   = cs;
				cpu->regs.ds   = ds;
				cpu->regs.es   = es;
				cpu->regs.ss   = ss;
				cpu->regs.scs  = cs;
				cpu->regs.sip  = ip;

			}


		} else if (line[0] == '@') {

			char *pval;

			u32 addr = strtoul(&line[1], &pval, 0);
			uint val = strtoul(pval, NULL, 0);

			*pval = 0;

			if (executed) test_expect(tr, line, ram_peek(cpu->mem.data, addr), val);
			else          ram_poke(cpu->mem.data, addr, val);


		} else if (line[0] == 'X') {

			while (true) {

				i8086_tick(cpu);

				if (cpu->insn.fetch_opcode && !cpu->insn.op_override)
					break;
			}

			executed = true;

		}

	}

	if (!first)
		test_complete(tr);

	fs_close();

	return true;

}



void test_summary(struct test_report *tr)
{

	printf("\n");

	if (tr->tests_failed > 0) printf(TEXT_FAIL "%s\n", tr->title);
	else                      printf(TEXT_PASS "%s\n", tr->title);

	printf("Cases:  %d, failed: %d, passed: %d\n",
		tr->tests_failed + tr->tests_passed,
		tr->tests_failed,
		tr->tests_passed);

	printf("Checks: %d, failed: %d, passed: %d\n",
		tr->conditions_failed + tr->conditions_passed,
		tr->conditions_failed,
		tr->conditions_passed);

}



void ioport_rdwr(void *data, u16 port, uint mode, uint *v)
{

	*v = 0xffffffffu;

}



int main(int argc, char **argv)
{

	struct ram   ram;
	struct i8086 cpu;

	ram_alloc(&ram, 1*1024*1024);
	i8086_init(&cpu);

	cpu.io.rd = &ioport_rdwr;
	cpu.io.wr = &ioport_rdwr;

	cpu.mem.data = &ram;
	cpu.mem.rd   = &ram_rd;
	cpu.mem.wr   = &ram_wr;

	struct test_report tr[argc];

	test_init(&tr[0], "Total");

	for (int n=1; n < argc; n++) {

		test_init(&tr[n], argv[n]);
		test_run_cases(&tr[n], &cpu, argv[n]);
		test_aggregate(&tr[0], &tr[n]);

	}

	printf("\n");
	printf("\n");
	for (int n=1; n < argc; n++) {

		if (tr[n].tests_failed > 0)
		test_summary(&tr[n]);

	}

	printf("\n");
	test_summary(&tr[0]);

}

