/*
 * mips_codegen.h
 *
 * Copyright (c) 2009 Ulrich Hecht
 * Copyright (c) 2016 modified by Dmitry Smagin
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE
 * OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */


#ifndef MIPS_CG_H
#define MIPS_CG_H

typedef enum {
	MIPSREG_V0 = 2,
	MIPSREG_V1,

	MIPSREG_A0 = 4,
	MIPSREG_A1,
	MIPSREG_A2,
	MIPSREG_A3,

	MIPSREG_T0 = 8,
	MIPSREG_T1,
	MIPSREG_T2,
	MIPSREG_T3,
	MIPSREG_T4,
	MIPSREG_T5,
	MIPSREG_T6,
	MIPSREG_T7,

	MIPSREG_RA = 0x1f,
	MIPSREG_S8 = 0x1e,
	MIPSREG_S0 = 0x10,
	MIPSREG_S1,
	MIPSREG_S2,
	MIPSREG_S3,
	MIPSREG_S4,
	MIPSREG_S5,
	MIPSREG_S6,
	MIPSREG_S7,
} MIPSReg;

#define TEMP_1 				MIPSREG_T0
#define TEMP_2 				MIPSREG_T1
#define TEMP_3 				MIPSREG_T2

/* PERM_REG_1 is used to store psxRegs struct address */
#define PERM_REG_1 			MIPSREG_S8

/* Crazy macro to calculate offset of the field in the structure */
#ifndef offsetof
#define offsetof(T,F) ((unsigned int)((char *)&((T *)0L)->F - (char *)0L))
#endif

/* GPR offset */
#define offGPR(rx)	offsetof(psxRegisters, GPR.r[rx])

/* CP0 offset */
#define offCP0(rx)	offsetof(psxRegisters, CP0.r[rx])

/* CP2C offset */
#define offCP2C(rx)	offsetof(psxRegisters, CP2C.r[rx])

#define off(field)	offsetof(psxRegisters, field)

#define write32(i) \
	do { *recMem++ = (u32)(i); } while (0)

#define PUSH(reg) \
do { \
	write32(0x27bdfffc); /* addiu sp, sp, -4 */ \
	write32(0xafa00000 | (reg << 16)); /* sw reg, sp(0) */ \
} while (0)

#define POP(reg) \
do { \
	write32(0x8fa00000 | (reg << 16)); /* lw reg, sp(0) */\
	write32(0x27bd0004); /* addiu sp, sp, 4 */ \
} while (0)

#define LW(rt, rn, imm) \
	write32(0x8c000000 | ((rn) << 21) | ((rt) << 16) | ((imm) & 0xffff))

#define SW(rd, rn, imm) \
	write32(0xac000000 | ((rn) << 21) | ((rd) << 16) | ((imm) & 0xffff))

#define LWL(rt, rn, imm) \
	write32(0x88000000 | ((rn) << 21) | ((rt) << 16) | ((imm) & 0xffff))

#define LWR(rt, rn, imm) \
	write32(0x98000000 | ((rn) << 21) | ((rt) << 16) | ((imm) & 0xffff))

#define SWL(rt, rn, imm) \
	write32(0xa8000000 | ((rn) << 21) | ((rt) << 16) | ((imm) & 0xffff))

#define SWR(rt, rn, imm) \
	write32(0xb8000000 | ((rn) << 21) | ((rt) << 16) | ((imm) & 0xffff))

#define ADDIU(rt, rs, imm) \
	write32(0x24000000 | ((rs) << 21) | ((rt) << 16) | ((imm) & 0xffff))

#define SLTI(rt, rs, imm16) \
	write32(0x28000000 | (rs << 21) | (rt << 16) | (imm16 & 0xffff))

#define SLTIU(rt, rs, imm16) \
	write32(0x2c000000 | (rs << 21) | (rt << 16) | (imm16 & 0xffff))

#define LUI(rt, imm16) \
	write32(0x3c000000 | (rt << 16) | ((imm16) & 0xffff))

#define LI16(reg, imm16) \
	write32(0x34000000 | ((reg) << 16) | ((short)imm16)) /* ori reg, zero, imm16 */

#define LI32(reg, imm32) \
do { \
	write32(0x3c000000 | (reg << 16) | ((imm32) >> 16)); /* lui */ \
	write32(0x34000000 | (reg << 21) | (reg << 16) | ((imm32) & 0xffff)); /* ori */ \
} while (0)

#define MOV(rd, rs) \
	write32(0x00000021 | ((rs) << 21) | ((rd) << 11)) /* move rd, rs */

#define ANDI(rt, rs, imm16) \
	write32(0x30000000 | (rs << 21) | (rt << 16) | (imm16 & 0xffff))

#define ORI(rt, rs, imm16) \
	write32(0x34000000 | (rs << 21) | (rt << 16) | (imm16 & 0xffff))

#define XORI(rt, rs, imm16) \
	write32(0x38000000 | ((rs) << 21) | ((rt) << 16) | ((imm16) & 0xffff))

#define XOR(rd, rn, rm) \
	write32(0x00000026 | ((rn) << 21) | ((rm) << 16) | ((rd << 11)))

#define SUBU(rd, rn, rm) \
	write32(0x00000023 | ((rn) << 21) | ((rm) << 16) | ((rd) << 11)) /* subu */

#define ADDU(rd, rn, rm) \
	write32(0x00000021 | ((rn) << 21) | ((rm) << 16) | ((rd) << 11)) /* addu */

#define AND(rd, rn, rm) \
	write32(0x00000024 | ((rn) << 21) | ((rm) << 16) | ((rd) << 11))

#define OR(rd, rn, rm) \
	write32(0x00000025 | ((rn) << 21) | ((rm) << 16) | ((rd) << 11))

#define NOR(rd, rs, rt) \
	write32(0x00000027 | (rs << 21) | (rt << 16) | (rd << 11))

#define SLL(rd, rt, sa) \
	write32(0x00000000 | (rt << 16) | (rd << 11) | ((sa & 31) << 6))

#define SRL(rd, rt, sa) \
	write32(0x00000002 | (rt << 16) | (rd << 11) | ((sa & 31) << 6))

#define SRA(rd, rt, sa) \
	write32(0x00000003 | (rt << 16) | (rd << 11) | ((sa & 31) << 6))

#define SLLV(rd, rt, rs) \
	write32(0x00000004 | (rs << 21) | (rt << 16) | (rd << 11))

#define SRLV(rd, rt, rs) \
	write32(0x00000006 | (rs << 21) | (rt << 16) | (rd << 11))

#define SRAV(rd, rt, rs) \
	write32(0x00000007 | (rs << 21) | (rt << 16) | (rd << 11))

#define MULT(rs, rt) \
	write32(0x00000018 | (rs << 21) | (rt << 16))

#define MULTU(rs, rt) \
	write32(0x00000019 | (rs << 21) | (rt << 16))

#define DIV(rs, rt) \
	write32(0x0000001a | (rs << 21) | (rt << 16))

#define DIVU(rs, rt) \
	write32(0x0000001b | (rs << 21) | (rt << 16))

#define MFLO(rd) \
	write32(0x00000012 | (rd << 11))

#define MFHI(rd) \
	write32(0x00000010 | (rd << 11))

#define SLT(rd, rs, rt) \
	write32(0x0000002a | (rs << 21) | (rt << 16) | (rd << 11))

#define SLTU(rd, rs, rt) \
	write32(0x0000002b | (rs << 21) | (rt << 16) | (rd << 11))

#define BEQ(rs, rt, offset) \
	write32(0x10000000 | (rs << 21) | (rt << 16) | (offset >> 2))

#define BEQZ(rs, offset)	BEQ(rs, 0, offset)
#define B(offset)		BEQ(0, 0, offset)

#define BNE(rs, rt, offset) \
	write32(0x14000000 | (rs << 21) | (rt << 16) | (offset >> 2))

#define NOP() \
	write32(0)

#define EXT(rt, rs, pos, size) \
	write32(0x7c000000 | (rs << 21) | (rt << 16) | \
	        ((pos & 0x1f) << 6) | (((size-1) & 0x1f) << 11))

#define INS(rt, rs, pos, size) \
	write32(0x7c000004 | (rs << 21) | (rt << 16) | \
	        ((pos & 0x1f) << 6) | (((pos+size-1) & 0x1f) << 11))

#define SEB(rd, rt) \
	write32(0x7C000420 | (rt << 16) | (rd << 11))

#define SEH(rd, rt) \
	write32(0x7C000620 | (rt << 16) | (rd << 11))

#define CLZ(rd, rs) \
	write32(0x70000020 | (rs << 21) | (rd << 16) | (rd << 11))

/* start of the recompiled block */
#define rec_recompile_start() \
do { \
	if (loadedpermregs == 0) { \
		LI32(PERM_REG_1, (u32)&psxRegs); \
		loadedpermregs = 1; \
	} \
} while (0)

/* end of the recompiled block */
#define rec_recompile_end() \
do { \
	write32(0x00000008 | (MIPSREG_V0 << 21)); /* jr v0 */ \
	write32(0); /* nop */ \
} while (0)

/* call func */
#define CALLFunc(func) \
do { \
	write32(0x0c000000 | ((func & 0x0fffffff) >> 2)); /* jal func */ \
	write32(0); /* nop */ \
} while (0)

#define mips_relative_offset(source, offset, next) \
	((((u32)(offset) - ((u32)(source) + (next))) >> 2) & 0xFFFF)

#define fixup_branch(BACKPATCH) \
do { \
	*( u32*)(BACKPATCH) |= mips_relative_offset(BACKPATCH, (u32)recMem, 4); \
} while (0)

#endif /* MIPS_CG_H */

