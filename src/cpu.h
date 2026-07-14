#include "common.h"

enum Banks {
	SYSUSR=0,
	FIQ=1,
	SVC=2,
	ABT=3,
	IRQ=4,
	UND=5,
	NBANKS=6,
};

enum Regs {
	SP=13,
	LR=14,
	PC=15,
};

enum Conditions {
	EQ = 0,
	NE = 1,
	CS = 2,
	CC = 3,
	MI = 4,
	PL = 5,
	VS = 6,
	VC = 7,
	HI = 8,
	LS = 9,
	GE = 10,
	LT = 11,
	GT = 12,
	LE = 13,
	AL = 14,
};

#define CPSR_T (1 << 5)
#define CPSR_MBITS 0b11111
#define CPSR_MBITS_USR 0b10000
#define CPSR_MBITS_FIQ 0b10001
#define CPSR_MBITS_IRQ 0b10010
#define CPSR_MBITS_SVC 0b10011
#define CPSR_MBITS_ABT 0b10111
#define CPSR_MBITS_UND 0b11011
#define CPSR_MBITS_SYS 0b11111

#define CONCAT_MASK(h, l) (((h) << 20) | (l) << 4)
#define CPSR_V 28
#define CPSR_C 29
#define CPSR_N 30
#define CPSR_Z 31
#define CPSR_SET(cpsr, field, bit) ((cpsr) & ~(1 << (field)) | ((bit) << (field)))
#define CPSR_GET(cpsr, field) (((cpsr) >> (field)) & 1)

struct cpu {
	u32 regs[NBANKS][16];
	u32 cpsr;
	u32 spsr[NBANKS];
};

static u32 *cpu_reg(struct cpu *c, int index) {
	switch (c->cpsr & CPSR_MBITS) {
		case CPSR_MBITS_USR:
		case CPSR_MBITS_SYS:
			return &c->regs[SYSUSR][index];
		case CPSR_MBITS_FIQ:
			if (index < 8)
				return &c->regs[SYSUSR][index];
			else
				return &c->regs[FIQ][index];
		case CPSR_MBITS_IRQ:
			if (index < 13)
				return &c->regs[SYSUSR][index];
			else
				return &c->regs[IRQ][index];
		case CPSR_MBITS_SVC:
			if (index < 13)
				return &c->regs[SYSUSR][index];
			else
				return &c->regs[SVC][index];
		case CPSR_MBITS_ABT:
			if (index < 13)
				return &c->regs[SYSUSR][index];
			else
				return &c->regs[ABT][index];
		case CPSR_MBITS_UND:
			if (index < 13)
				return &c->regs[SYSUSR][index];
			else
				return &c->regs[UND][index];
		default:
			abort();
	}
}

static bool check_condition(u32 cpsr, int code) {
	switch (code) {
		case EQ:
			return CPSR_GET(cpsr, CPSR_Z);
		case NE:
			return !CPSR_GET(cpsr, CPSR_Z);
		case CS:
			return CPSR_GET(cpsr, CPSR_C);
		case CC:
			return !CPSR_GET(cpsr, CPSR_C);
		case MI:
			return CPSR_GET(cpsr, CPSR_N);
		case PL:
			return !CPSR_GET(cpsr, CPSR_N);
		case VS:
			return CPSR_GET(cpsr, CPSR_V);
		case VC:
			return !CPSR_GET(cpsr, CPSR_V);
		case HI:
			return CPSR_GET(cpsr, CPSR_C) && !CPSR_GET(cpsr, CPSR_Z);
		case LS:
			return !CPSR_GET(cpsr, CPSR_C) || CPSR_GET(cpsr, CPSR_Z);
		case GE:
			return CPSR_GET(cpsr, CPSR_N) == CPSR_GET(cpsr, CPSR_V);
		case LT:
			return CPSR_GET(cpsr, CPSR_N) != CPSR_GET(cpsr, CPSR_V);
		case GT:
			return !CPSR_GET(cpsr, CPSR_Z) && (CPSR_GET(cpsr, CPSR_N) == CPSR_GET(cpsr, CPSR_V));
		case LE:
			return CPSR_GET(cpsr, CPSR_Z) || (CPSR_GET(cpsr, CPSR_N) != CPSR_GET(cpsr, CPSR_V));
		case AL:
			return true;
		default:
			// TODO check this
			return false;
	}
}

static void execute_arm(struct cpu *c, u32 instruction) {
	if (!check_condition(c->cpsr, instruction >> 28)) {
		return;
	}
	// MUL, MLA
	if ((instruction & CONCAT_MASK(0b11111100, 0b1111)) == CONCAT_MASK(0b00000000, 0b1001)) {
		int rd = ((instruction >> 16) & 0b1111);
		int rn = ((instruction >> 12) & 0b1111);
		int rs = ((instruction >> 8) & 0b1111);
		int rm = (instruction & 0b1111);
		bool a = (instruction >> 21) & 1;
		bool s = (instruction >> 20) & 1;
		
		u32 value = *cpu_reg(c, rm) * *cpu_reg(c, rs);
		if (a) {
			value += *cpu_reg(c, rn);
		}
		if (s) {
			c->cpsr = CPSR_SET(c->cpsr, CPSR_N, value >> 31);
			c->cpsr = CPSR_SET(c->cpsr, CPSR_Z, !!value);
			c->cpsr = CPSR_SET(c->cpsr, CPSR_C, 0); // Meaningless value (I don't want to implement this now)
		}
		*cpu_reg(c, rd) = value;
	}
	// MULL, MLAL
	else if ((instruction & CONCAT_MASK(0b11111000, 0b1111)) == CONCAT_MASK(0b00001000, 0b1001)) {
		int rdhi = ((instruction >> 16) & 0b1111);
		int rdlo = ((instruction >> 12) & 0b1111);
		int rs = ((instruction >> 8) & 0b1111);
		int rm = (instruction & 0b1111);
		bool u = (instruction >> 22) & 1;
		bool a = (instruction >> 21) & 1;
		bool s = (instruction >> 20) & 1;
		
		u64 value = u ? (i64)*cpu_reg(c, rm) * (i64)*cpu_reg(c, rs) : (u64)*cpu_reg(c, rm) * (u64)*cpu_reg(c, rs);
		if (a) {
			value += (((u64)*cpu_reg(c,rdhi)) << 32) | ((u64)*cpu_reg(c,rdlo)); 
		}
		if (s) {
			c->cpsr = CPSR_SET(c->cpsr, CPSR_N, value >> 63);
			c->cpsr = CPSR_SET(c->cpsr, CPSR_Z, !!value);
			c->cpsr = CPSR_SET(c->cpsr, CPSR_C, 0); // Meaningless value (I don't want to implement this now)
		}
		*cpu_reg(c, rdhi) = value >> 32;
		*cpu_reg(c, rdlo) = value;
	}
	// SWP
	else if ((instruction & CONCAT_MASK(0b11111011, 0b1111)) == CONCAT_MASK(0b00010000, 0b1001)) {
	}
	// LDRH, STRH
	else if ((instruction & CONCAT_MASK(0b11100000, 0b1111)) == CONCAT_MASK(0b00000000, 0b1011)) {
	}
	// LDRSB, LDRSH
	else if ((instruction & CONCAT_MASK(0b11100001, 0b1101)) == CONCAT_MASK(0b00000001, 0b1101)) {
	}
	// MRS
	else if ((instruction & CONCAT_MASK(0b11111011, 0b1111)) == CONCAT_MASK(0b00010000, 0b0000)) {
	}
	// MSR (register)
	else if ((instruction & CONCAT_MASK(0b11111011, 0b1111)) == CONCAT_MASK(0b00010010, 0b0000)) {
	}
	// MSR (immediate)
	else if ((instruction & CONCAT_MASK(0b11111011, 0b0000)) == CONCAT_MASK(0b00110010, 0b0000)) {
	}
	// BX
	else if ((instruction & CONCAT_MASK(0b11111111, 0b1111)) == CONCAT_MASK(0b00010010, 0b0001)) {
	}
	// Data Processing (immediate Shift)
	else if ((instruction & CONCAT_MASK(0b11100000, 0b0001)) == CONCAT_MASK(0b00000000, 0b0000)) {
	}
	// Data Processing (register Shift)
	else if ((instruction & CONCAT_MASK(0b11100000, 0b1001)) == CONCAT_MASK(0b00000000, 0b0001)) {
	}
	// Undefined instruction in Data Processing
	else if ((instruction & CONCAT_MASK(0b11111011, 0b0000)) == CONCAT_MASK(0b00110000, 0b0000)) {
	}
	// Data Processing (immediate Value)
	else if ((instruction & CONCAT_MASK(0b11100000, 0b0000)) == CONCAT_MASK(0b00100000, 0b0000)) {
	}
	// LDR, STR (immediate offset)
	else if ((instruction & CONCAT_MASK(0b11100000, 0b0000)) == CONCAT_MASK(0b01000000, 0b0000)) {
	}
	// LDR, STR (register offset)
	else if ((instruction & CONCAT_MASK(0b11100000, 0b0001)) == CONCAT_MASK(0b01100000, 0b0000)) {
	}
	// LDM, STM
	else if ((instruction & CONCAT_MASK(0b11100000, 0b0000)) == CONCAT_MASK(0b10000000, 0b0000)) {
	}
	// B, BL
	else if ((instruction & CONCAT_MASK(0b11100000, 0b0000)) == CONCAT_MASK(0b10100000, 0b0000)) {
	}
	// STC, LDC
	else if ((instruction & CONCAT_MASK(0b11100000, 0b0000)) == CONCAT_MASK(0b11000000, 0b0000)) {
	}
	// CDP
	else if ((instruction & CONCAT_MASK(0b11110000, 0b0001)) == CONCAT_MASK(0b11100000, 0b0000)) {
	}
	// MCR, MRC
	else if ((instruction & CONCAT_MASK(0b11110000, 0b0001)) == CONCAT_MASK(0b11100000, 0b0001)) {
	}
	// SWI
	else if ((instruction & CONCAT_MASK(0b11110000, 0b0000)) == CONCAT_MASK(0b11110000, 0b0000)) {
	}
}
