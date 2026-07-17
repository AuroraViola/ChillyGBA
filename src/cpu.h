#include "common.h"
#include "memory.h"

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

enum DpOpCode {
	DP_AND = 0,
	DP_EOR = 1,
	DP_SUB = 2,
	DP_RSB = 3,
	DP_ADD = 4,
	DP_ADC = 5,
	DP_SBC = 6,
	DP_RSC = 7,
	DP_TST = 8,
	DP_TEQ = 9,
	DP_CMP = 10,
	DP_CMN = 11,
	DP_ORR = 12,
	DP_MOV = 13,
	DP_BIC = 14,
	DP_MVN = 15,
};

#define CPSR_MBITS 0b11111
#define CPSR_MBITS_USR 0b10000
#define CPSR_MBITS_FIQ 0b10001
#define CPSR_MBITS_IRQ 0b10010
#define CPSR_MBITS_SVC 0b10011
#define CPSR_MBITS_ABT 0b10111
#define CPSR_MBITS_UND 0b11011
#define CPSR_MBITS_SYS 0b11111

#define CONCAT_MASK(h, l) (((h) << 20) | (l) << 4)
#define CPSR_T 5
#define CPSR_V 28
#define CPSR_C 29
#define CPSR_N 30
#define CPSR_Z 31
#define CPSR_SET(cpsr, field, bit) ((cpsr) & ~(1 << (field)) | ((bit) << (field)))
#define CPSR_GET(cpsr, field) (((cpsr) >> (field)) & 1)
#define ROTATE_LEFT_8(value, n) (((value) << (n)) | ((value) >> (8-(n))))
#define ROTATE_LEFT_16(value, n) (((value) << (n)) | ((value) >> (16-(n))))
#define ROTATE_LEFT_32(value, n) (((value) << (n)) | ((value) >> (32-(n))))
#define ROTATE_RIGHT_8(value, n) (((value) >> (n)) | ((value) << (8-(n))))
#define ROTATE_RIGHT_16(value, n) (((value) >> (n)) | ((value) << (16-(n))))
#define ROTATE_RIGHT_32(value, n) (((value) >> (n)) | ((value) << (32-(n))))
#define mmin(a,b) (((a) < (b)) ? (a) : (b))
#define mmax(a,b) (((a) > (b)) ? (a) : (b))

struct cpu {
	u32 regs[NBANKS][16];
	u32 cpsr;
	u32 spsr[NBANKS];
	u32 instr_queue[2];
};

static void flush_pipeline(struct cpu *c) {
	c->instr_queue[0] = 0xffffffff;
	c->instr_queue[1] = 0xffffffff;
}

static void init_cpu(struct cpu *c) {
	flush_pipeline(c);
}

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

static void execute_arm(struct cpu *c, u32 instruction, struct Memory *m) {
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
		int rn = ((instruction >> 16) & 0b1111);
		int rd = ((instruction >> 12) & 0b1111);
		int rm = (instruction & 0b1111);
		bool b = (instruction >> 22) & 1;

		if (b) {
			u8 value = mem_read_8(m, *cpu_reg(c, rn));
			mem_write_8(m, *cpu_reg(c, rn), *cpu_reg(c, rm));
			*cpu_reg(c, rd) = value;
		}
		else {
			u32 value = mem_read_32(m, *cpu_reg(c, rn));
			mem_write_32(m, *cpu_reg(c, rn), *cpu_reg(c, rm));
			*cpu_reg(c, rd) = value;
		}
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
		int rn = (instruction & 0xf);
		if (*cpu_reg(c, rn) & 1) {
			c->cpsr = CPSR_SET(c->cpsr, CPSR_T, 1);
		}
		*cpu_reg(c, 15) = *cpu_reg(c, rn);
		flush_pipeline(c);
	}
	// Undefined instruction in Data Processing
	else if ((instruction & CONCAT_MASK(0b11111011, 0b0000)) == CONCAT_MASK(0b00110000, 0b0000)) {
	}
	// Data Processing
	else if ((instruction & CONCAT_MASK(0b11100000, 0b0000)) == CONCAT_MASK(0b00100000, 0b0000) ||
			(instruction & CONCAT_MASK(0b11100000, 0b0001)) == CONCAT_MASK(0b00000000, 0b0000) ||
			(instruction & CONCAT_MASK(0b11100000, 0b1001)) == CONCAT_MASK(0b00000000, 0b0001)) {
		int opcode = (instruction >> 21) & 0b1111;
		int rn = ((instruction >> 16) & 0b1111);
		int rd = ((instruction >> 12) & 0b1111);
		int op2 = (instruction & 0xffffff);
		bool s = (instruction >> 20) & 1;
		bool i = (instruction >> 25) & 1;

		u32 operand2;
		if (i == 0) {
			u8 rm = op2 & 0xf;
			int shift = op2 >> 4;
			bool immediate_shift = (shift & 1) == 0;
			u8 shift_amount = immediate_shift ? (shift >> 3) : *cpu_reg(c, (shift >> 4) & 0xf);
			bool carry = CPSR_GET(c->cpsr, CPSR_C);
			switch ((shift >> 1) & 3) {
				case 0b00:
					shift_amount = mmin(shift_amount, 33);
					if (shift_amount != 0) {
						carry = (((u32)((u64)(*cpu_reg(c, rm)) << (shift_amount - 1))) >> 31) != 0;
						c->cpsr = CPSR_SET(c->cpsr, CPSR_C, carry);
					}
					operand2 = (u32)((u64)(*cpu_reg(c, rm)) << shift_amount);
					break;
				case 0b01:
					if (immediate_shift && shift_amount == 0) {
						shift_amount = 32;
					}
					shift_amount = mmin(shift_amount, 33);
					if (shift_amount != 0) {
						carry = (((u64)(*cpu_reg(c, rm)) >> (shift_amount-1)) & 1) != 0;
					}
					operand2 = (u64)(((u64)*cpu_reg(c, rm)) >> shift_amount);
					break;
				case 0b10:
					if (immediate_shift && shift_amount == 0) {
						shift_amount = 32;
					}
					shift_amount = mmin(shift_amount, 33);
					if (shift_amount != 0) {
						carry = (((i64)((i32)(*cpu_reg(c, rm)))) >> (shift_amount -1) & 1) != 0;
						c->cpsr = CPSR_SET(c->cpsr, CPSR_C, carry);
					}
					operand2 = (((i64)((i32)*cpu_reg(c, rm))) >> shift_amount);
					break;
				case 0b11:
					if (immediate_shift && shift_amount == 0) {
						bool tmp_carry = (*cpu_reg(c, rm) & 1) != 0;
						operand2 = (*cpu_reg(c, rm) >> 1) | ((carry ? 1 : 0) << 31);
						c->cpsr = CPSR_SET(c->cpsr, CPSR_C, tmp_carry);
					}
					else {
						if (shift_amount != 0) {
							shift_amount = shift_amount & 31;
							operand2 = (*cpu_reg(c, rm) >> shift_amount) | (*cpu_reg(c, rm) << ((32 - shift_amount) & 31));
							carry = ((operand2 >> 31) & 1) != 0;
							c->cpsr = CPSR_SET(c->cpsr, CPSR_C, carry);
						}
						else {
							operand2 = *cpu_reg(c, rm);
						}
					}
					break;
				default:
					abort();
			}
		}
		else {
			u32 imm = op2 & 0xff;
			u8 rotate = op2 >> 8;
			operand2 = ROTATE_RIGHT_32(imm, (rotate << 1));
			if (rotate != 0) {
				c->cpsr = CPSR_SET(c->cpsr, CPSR_C, ((imm >> ((rotate << 1) - 1) & 1) != 0));
			}
		}
		u32 result;
		u64 result64;
		u32 op3 = CPSR_GET(c->cpsr, CPSR_C) ^ 1;
		switch (opcode) {
			case DP_AND:
				*cpu_reg(c, rd) = *cpu_reg(c, rn) & operand2;
				c->cpsr = CPSR_SET(c->cpsr, CPSR_N, *cpu_reg(c, rd) >> 31);
				c->cpsr = CPSR_SET(c->cpsr, CPSR_Z, !!*cpu_reg(c,rd));
				break;
			case DP_EOR:
				*cpu_reg(c, rd) = *cpu_reg(c, rn) ^ operand2;
				c->cpsr = CPSR_SET(c->cpsr, CPSR_N, *cpu_reg(c, rd) >> 31);
				c->cpsr = CPSR_SET(c->cpsr, CPSR_Z, !!*cpu_reg(c,rd));
				break;
			case DP_SUB:
				result = *cpu_reg(c, rn) - operand2;
				if (s) {
					c->cpsr = CPSR_SET(c->cpsr, CPSR_C, *cpu_reg(c, rn) >= operand2);
					c->cpsr = CPSR_SET(c->cpsr, CPSR_V, (((*cpu_reg(c, rn) ^ operand2) & ((*cpu_reg(c, rn) ^ result) >> 31)) != 0));
				}
				*cpu_reg(c, rd) = result;
				c->cpsr = CPSR_SET(c->cpsr, CPSR_N, *cpu_reg(c, rd) >> 31);
				c->cpsr = CPSR_SET(c->cpsr, CPSR_Z, !!*cpu_reg(c,rd));
				break;
			case DP_RSB:
				result = operand2 - *cpu_reg(c, rn);
				if (s) {
					c->cpsr = CPSR_SET(c->cpsr, CPSR_C, *cpu_reg(c, rn) <= operand2);
					c->cpsr = CPSR_SET(c->cpsr, CPSR_V, (((operand2 ^ *cpu_reg(c, rn)) & ((operand2 ^ result) >> 31)) != 0));
				}
				*cpu_reg(c, rd) = result;
				c->cpsr = CPSR_SET(c->cpsr, CPSR_N, *cpu_reg(c, rd) >> 31);
				c->cpsr = CPSR_SET(c->cpsr, CPSR_Z, !!*cpu_reg(c,rd));
				break;
			case DP_ADD:
				result = *cpu_reg(c, rn) + operand2;
				if (s) {
					c->cpsr = CPSR_SET(c->cpsr, CPSR_C, *cpu_reg(c, rn) > result);
					c->cpsr = CPSR_SET(c->cpsr, CPSR_V, (((*cpu_reg(c, rn) ^ operand2) & ((operand2 ^ result) >> 31)) != 0));
				}
				*cpu_reg(c, rd) = result;
				c->cpsr = CPSR_SET(c->cpsr, CPSR_N, *cpu_reg(c, rd) >> 31);
				c->cpsr = CPSR_SET(c->cpsr, CPSR_Z, !!*cpu_reg(c,rd));
				break;
			case DP_ADC:
				result64 = *cpu_reg(c, rn) + operand2 + CPSR_GET(c->cpsr, CPSR_C);
				if (s) {
					c->cpsr = CPSR_SET(c->cpsr, CPSR_C, result64 >> 32 != 0);
					c->cpsr = CPSR_SET(c->cpsr, CPSR_V, (((*cpu_reg(c, rn) ^ operand2) & ((operand2 ^ ((u32)result64)) >> 31)) != 0));
				}
				*cpu_reg(c, rd) = (u32)result64;
				c->cpsr = CPSR_SET(c->cpsr, CPSR_N, *cpu_reg(c, rd) >> 31);
				c->cpsr = CPSR_SET(c->cpsr, CPSR_Z, !!*cpu_reg(c,rd));
				break;
			case DP_SBC:
				result = *cpu_reg(c, rn) - operand2 - op3;
				c->cpsr = CPSR_SET(c->cpsr, CPSR_N, result >> 31);
				c->cpsr = CPSR_SET(c->cpsr, CPSR_Z, !!result);
				if (s) {
					c->cpsr = CPSR_SET(c->cpsr, CPSR_C, (u64)*cpu_reg(c,rn) >= ((u64)operand2 + (u64)op3));
					c->cpsr = CPSR_SET(c->cpsr, CPSR_V, (((*cpu_reg(c, rn) ^ operand2) & ((*cpu_reg(c, rn) ^ result) >> 31)) != 0));
				}
				*cpu_reg(c, rd) = result;
				break;
			case DP_RSC:
				result = operand2 - *cpu_reg(c, rn) - op3;
				c->cpsr = CPSR_SET(c->cpsr, CPSR_N, result >> 31);
				c->cpsr = CPSR_SET(c->cpsr, CPSR_Z, !!result);
				if (s) {
					c->cpsr = CPSR_SET(c->cpsr, CPSR_C, (u64)operand2 >= ((u64)*cpu_reg(c,rn) + (u64)op3));
					c->cpsr = CPSR_SET(c->cpsr, CPSR_V, (((operand2 ^ *cpu_reg(c, rn)) & ((operand2 ^ result) >> 31)) != 0));
				}
				*cpu_reg(c, rd) = result;
				break;
			case DP_TST:
				result = *cpu_reg(c, rn) & operand2;
				c->cpsr = CPSR_SET(c->cpsr, CPSR_N, result >> 31);
				c->cpsr = CPSR_SET(c->cpsr, CPSR_Z, !!result);
				break;
			case DP_TEQ:
				result = *cpu_reg(c, rn) ^ operand2;
				c->cpsr = CPSR_SET(c->cpsr, CPSR_N, result >> 31);
				c->cpsr = CPSR_SET(c->cpsr, CPSR_Z, !!result);
				break;
			case DP_CMP:
				result = *cpu_reg(c, rn) - operand2;
				c->cpsr = CPSR_SET(c->cpsr, CPSR_N, result >> 31);
				c->cpsr = CPSR_SET(c->cpsr, CPSR_Z, !!result);
				if (s) {
					c->cpsr = CPSR_SET(c->cpsr, CPSR_C, *cpu_reg(c, rn) >= operand2);
					c->cpsr = CPSR_SET(c->cpsr, CPSR_V, (((*cpu_reg(c, rn) ^ operand2) & ((*cpu_reg(c, rn) ^ result) >> 31)) != 0));
				}
				break;
			case DP_CMN:
				result = *cpu_reg(c, rn) + operand2;
				c->cpsr = CPSR_SET(c->cpsr, CPSR_N, result >> 31);
				c->cpsr = CPSR_SET(c->cpsr, CPSR_Z, !!result);
				if (s) {
					c->cpsr = CPSR_SET(c->cpsr, CPSR_C, *cpu_reg(c, rn) > result);
					c->cpsr = CPSR_SET(c->cpsr, CPSR_V, (((*cpu_reg(c, rn) ^ operand2) & ((operand2 ^ result) >> 31)) != 0));
				}
				break;
			case DP_ORR:
				*cpu_reg(c, rd) = *cpu_reg(c, rn) | operand2;
				c->cpsr = CPSR_SET(c->cpsr, CPSR_N, *cpu_reg(c, rd) >> 31);
				c->cpsr = CPSR_SET(c->cpsr, CPSR_Z, !!*cpu_reg(c,rd));
				break;
			case DP_MOV:
				*cpu_reg(c, rd) = operand2;
				c->cpsr = CPSR_SET(c->cpsr, CPSR_N, *cpu_reg(c, rd) >> 31);
				c->cpsr = CPSR_SET(c->cpsr, CPSR_Z, !!*cpu_reg(c,rd));
				break;
			case DP_BIC:
				*cpu_reg(c, rd) = *cpu_reg(c, rn) & ~operand2;
				c->cpsr = CPSR_SET(c->cpsr, CPSR_N, *cpu_reg(c, rd) >> 31);
				c->cpsr = CPSR_SET(c->cpsr, CPSR_Z, !!*cpu_reg(c,rd));
				break;
			case DP_MVN:
				*cpu_reg(c, rd) = ~operand2;
				c->cpsr = CPSR_SET(c->cpsr, CPSR_N, *cpu_reg(c, rd) >> 31);
				c->cpsr = CPSR_SET(c->cpsr, CPSR_Z, !!*cpu_reg(c,rd));
				break;
		}
		if (rd == 15) {
			*cpu_reg(c, rd) += 4;
		}
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
		int offset = (instruction & 0xffffff);
		bool l = (instruction >> 24) & 1;
		offset |= (offset >> 23) ? 0xff000000 : 0;
		// TODO check if PC offsets are correct
		if (l) {
			*cpu_reg(c, 14) = *cpu_reg(c, 15) - 4;
		}
		*cpu_reg(c, 15) += offset - 8;
		flush_pipeline(c);
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
