// Generate inline psxMemRead/Write or call them as-is
#define USE_DIRECT_MEM_ACCESS
#define USE_CONST_ADDRESSES

#define OPCODE(insn, rt, rn, imm) \
	write32((insn) | ((rn) << 21) | ((rt) << 16) | ((imm) & 0xffff))

//#define LOG_WL_WR
//#define LOG_LOADS
//#define LOG_STORES

static void disasm_psx(u32 pc)
{
	static char buffer[512];
	u32 opcode = *(u32 *)((char *)PSXM(pc));
	disasm_mips_instruction(opcode, buffer, pc, 0, 0);
	printf("%08x: %08x %s\n", pc, opcode, buffer);
}

s32 imm_max, imm_min;

/* NOTE: psxM must be mmap'ed, not malloc'ed, otherwise segfault */
static int LoadFromConstAddr(int count)
{
#ifdef USE_CONST_ADDRESSES
	if (IsConst(_Rs_)) {
		u32 addr = iRegs[_Rs_].r + imm_min;

		/* Check if addr and addr+imm are in the same 2MB region */
		if ((addr & 0xe00000) != (iRegs[_Rs_].r & 0xe00000))
			return 0;

		if ((addr & 0x1fffffff) < 0x800000) {
			u32 r2 = regMipsToHost(_Rs_, REG_LOAD, REG_REGISTER);
			u32 PC = pc - 4;

			#ifdef WITH_DISASM
			for (int i = 0; i < count-1; i++)
				DISASM_PSX(pc + i * 4);
			#endif

			if ((u32)psxM == 0x10000000) {
				LUI(TEMP_2, 0x1000);
				INS(TEMP_2, r2, 0, 0x15);
			} else {
				LW(TEMP_2, PERM_REG_1, off(psxM));
				EXT(TEMP_1, r2, 0, 0x15);
				ADDU(TEMP_2, TEMP_2, TEMP_1);
			}

			do {
				u32 opcode = *(u32 *)((char *)PSXM(PC));
				s32 imm = _fImm_(opcode);
				u32 rt = _fRt_(opcode);
				u32 r1 = regMipsToHost(rt, REG_FIND, REG_REGISTER);

				OPCODE(opcode & 0xfc000000, r1, TEMP_2, imm);

				SetUndef(rt);
				regMipsChanged(rt);
				regUnlock(r1);
				PC += 4;
			} while (--count);

			pc = PC;
			regUnlock(r2);

			return 1;
		}
	}
#endif

	return 0;
}

static int StoreToConstAddr(int count)
{
#ifdef USE_CONST_ADDRESSES
	if (IsConst(_Rs_)) {
		u32 addr = iRegs[_Rs_].r + imm_min;

		/* Check if addr and addr+imm are in the same 2MB region */
		if ((addr & 0xe00000) != (iRegs[_Rs_].r & 0xe00000))
			return 0;

		if ((addr & 0x1fffffff) < 0x800000) {
			u32 r2 = regMipsToHost(_Rs_, REG_LOAD, REG_REGISTER);
			u32 PC = pc - 4;

			#ifdef WITH_DISASM
			for (int i = 0; i < count-1; i++)
				DISASM_PSX(pc + i * 4);
			#endif

			if ((u32)psxM == 0x10000000) {
				LUI(TEMP_2, 0x1000);
				INS(TEMP_2, r2, 0, 0x15);
			} else {
				LW(TEMP_2, PERM_REG_1, off(psxM));
				EXT(TEMP_1, r2, 0, 0x15);
				ADDU(TEMP_2, TEMP_2, TEMP_1);
			}

			do {
				u32 opcode = *(u32 *)((char *)PSXM(PC));
				s32 imm = _fImm_(opcode);
				u32 rt = _fRt_(opcode);
				u32 r1 = regMipsToHost(rt, REG_LOAD, REG_REGISTER);

				OPCODE(opcode & 0xfc000000, r1, TEMP_2, imm);

				regUnlock(r1);
				PC += 4;
			} while (--count);

			pc = PC;
			regUnlock(r2);
			return 1;
		}
	}
#endif

	return 0;
}

static void LoadFromAddr(int count)
{
	// Rt = [Rs + imm16]
	int icount = count;
	u32 r1 = regMipsToHost(_Rs_, REG_LOAD, REG_REGISTER);
	u32 PC = pc - 4;
	u32 *backpatch1, *backpatch2, *backpatch4;

	#ifdef WITH_DISASM
	for (int i = 0; i < count-1; i++)
		DISASM_PSX(pc + i * 4);
	#endif

#ifdef USE_DIRECT_MEM_ACCESS
	regPushState();

	ADDIU(MIPSREG_A0, r1, imm_min);
	EXT(TEMP_1, MIPSREG_A0, 0, 0x1d); // and 0x1fffffff
	LUI(TEMP_2, 0x80);
	SLTU(TEMP_3, TEMP_1, TEMP_2);

	backpatch1 = (u32 *)recMem;
	BEQZ(TEMP_3, 0); // beqz temp_2, label_hle
	NOP();

	/* Check if addr and addr+imm are in the same 2MB region */
	LUI(TEMP_1, 0xe0);
	AND(TEMP_2, MIPSREG_A0, TEMP_1);
	AND(TEMP_3, r1, TEMP_1);

	backpatch4 = (u32 *)recMem;
	BNE(TEMP_2, TEMP_3, 0); // bne temp_2, temp_3, label_hle
	NOP();

	EXT(TEMP_1, r1, 0, 0x15); // and 0x1fffff

	if ((u32)psxM == 0x10000000)
		LUI(TEMP_2, 0x1000);
	else
		LW(TEMP_2, PERM_REG_1, off(psxM));

	ADDU(TEMP_2, TEMP_2, TEMP_1);

	do {
		u32 opcode = *(u32 *)((char *)PSXM(PC));
		s32 imm = _fImm_(opcode);
		u32 rt = _fRt_(opcode);
		u32 r2 = regMipsToHost(rt, REG_FIND, REG_REGISTER);

		OPCODE(opcode & 0xfc000000, r2, TEMP_2, imm);

		SetUndef(rt);
		regMipsChanged(rt);
		regUnlock(r2);

		PC += 4;
	} while (--icount);

	backpatch2 = (u32 *)recMem;
	B(0); // b label_exit
	NOP();

	PC = pc - 4;

	regPopState();

	// label_hle:
	fixup_branch(backpatch1);
	fixup_branch(backpatch4);
#endif

	do {
		u32 opcode = *(u32 *)((char *)PSXM(PC));
		u32 rt = _fRt_(opcode);
		s32 imm = _fImm_(opcode);
		u32 r2 = regMipsToHost(rt, REG_FIND, REG_REGISTER);

		ADDIU(MIPSREG_A0, r1, imm);

		switch (opcode & 0xfc000000) {
		case 0x80000000: CALLFunc((u32)psxMemRead8); SEB(r2, MIPSREG_V0); break; // LB
		case 0x90000000: CALLFunc((u32)psxMemRead8); MOV(r2, MIPSREG_V0); break; // LBU
		case 0x84000000: CALLFunc((u32)psxMemRead16); SEH(r2, MIPSREG_V0); break; // LH
		case 0x94000000: CALLFunc((u32)psxMemRead16); MOV(r2, MIPSREG_V0); break; // LHU
		case 0x8c000000: CALLFunc((u32)psxMemRead32); MOV(r2, MIPSREG_V0); break; // LW
		}

		SetUndef(rt);
		regMipsChanged(rt);
		regUnlock(r2);

		PC += 4;
	} while (--count);

#ifdef USE_DIRECT_MEM_ACCESS
	// label_exit:
	fixup_branch(backpatch2);
#endif

	pc = PC;

	regUnlock(r1);
}

static void StoreToAddr(int count)
{
	int icount = count;
	u32 r1 = regMipsToHost(_Rs_, REG_LOAD, REG_REGISTER);
	u32 PC = pc - 4;
	u32 *backpatch1, *backpatch2, *backpatch3, *backpatch4;

	#ifdef WITH_DISASM
	for (int i = 0; i < count-1; i++)
		DISASM_PSX(pc + i * 4);
	#endif

#ifdef USE_DIRECT_MEM_ACCESS
	regPushState();

	/* First check if memory is writable atm */
	LW(TEMP_1, PERM_REG_1, off(writeok));
	backpatch3 = (u32 *)recMem;
	BEQZ(TEMP_1, 0); // beqz temp_1, label_hle
	NOP();

	ADDIU(MIPSREG_A0, r1, imm_min);
	EXT(TEMP_1, MIPSREG_A0, 0, 0x1d); // and 0x1fffffff
	LUI(TEMP_2, 0x80);
	SLTU(TEMP_3, TEMP_1, TEMP_2);
	backpatch1 = (u32 *)recMem;
	BEQZ(TEMP_3, 0); // beqz temp_2, label_hle
	NOP();

	/* Check if addr and addr+imm are in the same 2MB region */
	LUI(TEMP_1, 0xe0);
	AND(TEMP_2, MIPSREG_A0, TEMP_1);
	AND(TEMP_3, r1, TEMP_1);

	backpatch4 = (u32 *)recMem;
	BNE(TEMP_2, TEMP_3, 0); // bne temp_2, temp_3, label_hle
	NOP();

	EXT(TEMP_1, r1, 0, 0x15); // and 0x1fffff

	if ((u32)psxM == 0x10000000)
		LUI(TEMP_2, 0x1000);
	else
		LW(TEMP_2, PERM_REG_1, off(psxM));

	ADDU(TEMP_2, TEMP_2, TEMP_1);

	do {
		u32 opcode = *(u32 *)((char *)PSXM(PC));
		s32 imm = _fImm_(opcode);
		u32 rt = _fRt_(opcode);
		u32 r2 = regMipsToHost(rt, REG_LOAD, REG_REGISTER);

		OPCODE(opcode & 0xfc000000, r2, TEMP_2, imm);

		/* Invalidate recRAM[addr+imm16] pointer */
		EXT(TEMP_1, MIPSREG_A0, 0, 0x15); // and 0x1fffff
		INS(TEMP_1, 0, 0, 2); // clear 2 lower bits
		LW(MIPSREG_V0, PERM_REG_1, off(reserved));
		ADDU(TEMP_1, TEMP_1, MIPSREG_V0);
		SW(0, TEMP_1, 0);

		PC += 4;

		regUnlock(r2);
	} while (--icount);

	backpatch2 = (u32 *)recMem;
	B(0); // b label_exit
	NOP();

	PC = pc - 4;

	regPopState();

	// label_hle:
	fixup_branch(backpatch1);
	fixup_branch(backpatch3);
	fixup_branch(backpatch4);
#endif

	do {
		u32 opcode = *(u32 *)((char *)PSXM(PC));
		u32 rt = _fRt_(opcode);
		s32 imm = _fImm_(opcode);
		u32 r2 = regMipsToHost(rt, REG_LOAD, REG_REGISTER);

		ADDIU(MIPSREG_A0, r1, imm);
		MOV(MIPSREG_A1, r2);
		switch (opcode & 0xfc000000) {
		case 0xa0000000: CALLFunc((u32)psxMemWrite8); break;
		case 0xa4000000: CALLFunc((u32)psxMemWrite16); break;
		case 0xac000000: CALLFunc((u32)psxMemWrite32); break;
		default: break;
		}

		PC += 4;

		regUnlock(r2);
	} while (--count);

#ifdef USE_DIRECT_MEM_ACCESS
	// label_exit:
	fixup_branch(backpatch2);
#endif

	pc = PC;

	regUnlock(r1);
}

static int calc_loads()
{
	int count = 0;
	u32 PC = pc;
	u32 opcode = psxRegs.code;
	u32 rs = _Rs_;

	imm_min = imm_max = _fImm_(opcode);

	/* If in delay slot, set count to 1 */
	if (branch)
		return 1;

	/* Allow LB, LBU, LH, LHU and LW */
	/* rs should be the same, imm and rt could be different */
	while ((_fOp_(opcode) == 0x20 || _fOp_(opcode) == 0x24 ||
	        _fOp_(opcode) == 0x21 || _fOp_(opcode) == 0x25 ||
	        _fOp_(opcode) == 0x23) && (rs == _fRs_(opcode))) {

		/* Update min and max immediate values */
		if (_fImm_(opcode) > imm_max) imm_max = _fImm_(opcode);
		if (_fImm_(opcode) < imm_min) imm_min = _fImm_(opcode);

		opcode = *(u32 *)((char *)PSXM(PC));

		PC += 4;
		count++;

		/* Extra paranoid check if rt == rs */
		if (_fRt_(opcode) == _fRs_(opcode))
			return count;

	}

#ifdef LOG_LOADS
	if (count) {
		printf("\nFOUND %d loads, min: %d, max: %d\n", count, imm_min, imm_max);
		u32 dpc = pc - 4;
		for (; dpc < PC - 4; dpc += 4)
			disasm_psx(dpc);
	}
#endif

	return count;
}

static int calc_stores()
{
	int count = 0;
	u32 PC = pc;
	u32 opcode = psxRegs.code;
	u32 rs = _Rs_;

	imm_min = imm_max = _fImm_(opcode);

	/* If in delay slot, set count to 1 */
	if (branch)
		return 1;

	/* Allow SB, SH and SW */
	/* rs should be the same, imm and rt could be different */
	while (((_fOp_(opcode) == 0x28) || (_fOp_(opcode) == 0x29) ||
	        (_fOp_(opcode) == 0x2b)) && (rs == _fRs_(opcode))) {

		/* Update min and max immediate values */
		if (_fImm_(opcode) > imm_max) imm_max = _fImm_(opcode);
		if (_fImm_(opcode) < imm_min) imm_min = _fImm_(opcode);

		opcode = *(u32 *)((char *)PSXM(PC));

		PC += 4;
		count++;
	}

#ifdef LOG_STORES
	if (count) {
		printf("\nFOUND %d stores, min: %d, max: %d\n", count, imm_min, imm_max);
		u32 dpc = pc - 4;
		for (; dpc < PC - 4; dpc += 4)
			disasm_psx(dpc);
	}
#endif

	return count;
}

static void recLB()
{
	int count = calc_loads();

	if (autobias) cycles_pending += count;

	// Rt = mem[Rs + Im] (signed)
	if (LoadFromConstAddr(count))
		return;

	LoadFromAddr(count);
}

static void recLBU()
{
	int count = calc_loads();

	if (autobias) cycles_pending += count;

	// Rt = mem[Rs + Im] (unsigned)
	if (LoadFromConstAddr(count))
		return;

	LoadFromAddr(count);
}

static void recLH()
{
	int count = calc_loads();

	if (autobias) cycles_pending += count;

	// Rt = mem[Rs + Im] (signed)
	if (LoadFromConstAddr(count))
		return;

	LoadFromAddr(count);
}

static void recLHU()
{
	int count = calc_loads();

	if (autobias) cycles_pending += count;

	// Rt = mem[Rs + Im] (unsigned)
	if (LoadFromConstAddr(count))
		return;

	LoadFromAddr(count);
}

static void recLW()
{
	int count = calc_loads();

	if (autobias) cycles_pending += count;

	// Rt = mem[Rs + Im] (unsigned)
	if (LoadFromConstAddr(count))
		return;

	LoadFromAddr(count);
}

static void recSB()
{
	int count = calc_stores();

	if (autobias) cycles_pending += count;

	// mem[Rs + Im] = Rt
	if (StoreToConstAddr(count))
		return;

	StoreToAddr(count);
}

static void recSH()
{
	int count = calc_stores();

	if (autobias) cycles_pending += count;

	// mem[Rs + Im] = Rt
	if (StoreToConstAddr(count))
		return;

	StoreToAddr(count);
}

static void recSW()
{
	int count = calc_stores();

	if (autobias) cycles_pending += count;

	// mem[Rs + Im] = Rt
	if (StoreToConstAddr(count))
		return;

	StoreToAddr(count);
}

static u32 LWL_MASK[4] = { 0xffffff, 0xffff, 0xff, 0 };
static u32 LWL_SHIFT[4] = { 24, 16, 8, 0 };
static u32 LWR_MASK[4] = { 0, 0xff000000, 0xffff0000, 0xffffff00 };
static u32 LWR_SHIFT[4] = { 0, 8, 16, 24 };

static void gen_LWL_LWR(int count)
{
	int icount = count;
	u32 r1 = regMipsToHost(_Rs_, REG_LOAD, REG_REGISTER);
	u32 PC = pc - 4;
	u32 *backpatch1, *backpatch2;

	#ifdef WITH_DISASM
	for (int i = 0; i < count-1; i++)
		DISASM_PSX(pc + i * 4);
	#endif

#ifdef USE_DIRECT_MEM_ACCESS
	regPushState();

	ADDIU(MIPSREG_A0, r1, imm_min);
	EXT(TEMP_1, MIPSREG_A0, 0, 0x1d); // and 0x1fffffff
	LUI(TEMP_2, 0x80);
	SLTU(TEMP_3, TEMP_1, TEMP_2);
	backpatch1 = (u32 *)recMem;
	BEQZ(TEMP_3, 0); // beqz temp_2, label_hle
	NOP();

	EXT(TEMP_1, r1, 0, 0x15); // and 0x1fffff

	if ((u32)psxM == 0x10000000)
		LUI(TEMP_2, 0x1000);
	else
		LW(TEMP_2, PERM_REG_1, off(psxM));

	ADDU(TEMP_2, TEMP_2, TEMP_1);

	do {
		u32 opcode = *(u32 *)((char *)PSXM(PC));
		s32 imm = _fImm_(opcode);
		u32 rt = _fRt_(opcode);
		u32 r2 = regMipsToHost(rt, REG_LOAD, REG_REGISTER);

		OPCODE(opcode & 0xfc000000, r2, TEMP_2, imm);

		SetUndef(rt);
		regMipsChanged(rt);
		regUnlock(r2);

		PC += 4;
	} while (--icount);

	backpatch2 = (u32 *)recMem;
	B(0); // b label_exit
	NOP();

	PC = pc - 4;

	regPopState();

	// label_hle:
	fixup_branch(backpatch1);
#endif

	do {
		u32 opcode = *(u32 *)((char *)PSXM(PC));
		u32 insn = opcode & 0xfc000000;
		u32 rt = _fRt_(opcode);
		s32 imm = _fImm_(opcode);
		u32 r2 = regMipsToHost(rt, REG_LOAD, REG_REGISTER);

		ADDIU(MIPSREG_A0, r1, imm); // a0 = r1 & ~3
		INS(MIPSREG_A0, 0, 0, 2); // clear 2 lower bits
		CALLFunc((u32)psxMemRead32); // result in MIPSREG_V0

		ADDIU(TEMP_1, r1, imm);
		ANDI(TEMP_1, TEMP_1, 3); // shift = addr & 3
		SLL(TEMP_1, TEMP_1, 2);

		if (insn == 0x88000000) // LWL
			LI32(TEMP_2, (u32)LWL_MASK);
		else			// LWR
			LI32(TEMP_2, (u32)LWR_MASK);

		ADDU(TEMP_2, TEMP_2, TEMP_1);
		LW(TEMP_2, TEMP_2, 0);
		AND(r2, r2, TEMP_2);

		if (insn == 0x88000000) // LWL
			LI32(TEMP_2, (u32)LWL_SHIFT);
		else			// LWR
			LI32(TEMP_2, (u32)LWR_SHIFT);

		ADDU(TEMP_2, TEMP_2, TEMP_1);
		LW(TEMP_2, TEMP_2, 0);

		if (insn == 0x88000000) // LWL
			SLLV(TEMP_3, MIPSREG_V0, TEMP_2);
		else			// LWR
			SRLV(TEMP_3, MIPSREG_V0, TEMP_2);

		OR(r2, r2, TEMP_3);

		SetUndef(rt);
		regMipsChanged(rt);
		regUnlock(r2);

		PC += 4;
	} while (--count);

#ifdef USE_DIRECT_MEM_ACCESS
	// label_exit:
	fixup_branch(backpatch2);
#endif

	pc = PC;

	regUnlock(r1);
}

static u32 SWL_MASK[4] = { 0xffffff00, 0xffff0000, 0xff000000, 0 };
static u32 SWL_SHIFT[4] = { 24, 16, 8, 0 };
static u32 SWR_MASK[4] = { 0, 0xff, 0xffff, 0xffffff };
static u32 SWR_SHIFT[4] = { 0, 8, 16, 24 };

static void gen_SWL_SWR(int count)
{
	int icount = count;
	u32 r1 = regMipsToHost(_Rs_, REG_LOAD, REG_REGISTER);
	u32 PC = pc - 4;
	u32 *backpatch1, *backpatch2, *backpatch3;

	#ifdef WITH_DISASM
	for (int i = 0; i < count-1; i++)
		DISASM_PSX(pc + i * 4);
	#endif

#ifdef USE_DIRECT_MEM_ACCESS
	regPushState();

	/* First check if memory is writable atm */
	LW(TEMP_1, PERM_REG_1, off(writeok));
	backpatch3 = (u32 *)recMem;
	BEQZ(TEMP_1, 0); // beqz temp_1, label_hle
	NOP();

	ADDIU(MIPSREG_A0, r1, imm_min);
	EXT(TEMP_1, MIPSREG_A0, 0, 0x1d); // and 0x1fffffff
	LUI(TEMP_2, 0x80);
	SLTU(TEMP_3, TEMP_1, TEMP_2);
	backpatch1 = (u32 *)recMem;
	BEQZ(TEMP_3, 0); // beqz temp_2, label_hle
	NOP();

	EXT(TEMP_1, r1, 0, 0x15); // and 0x1fffff

	if ((u32)psxM == 0x10000000)
		LUI(TEMP_2, 0x1000);
	else
		LW(TEMP_2, PERM_REG_1, off(psxM));

	ADDU(TEMP_2, TEMP_2, TEMP_1);

	do {
		u32 opcode = *(u32 *)((char *)PSXM(PC));
		s32 imm = _fImm_(opcode);
		u32 rt = _fRt_(opcode);
		u32 r2 = regMipsToHost(rt, REG_LOAD, REG_REGISTER);

		OPCODE(opcode & 0xfc000000, r2, TEMP_2, imm);

		PC += 4;

		regUnlock(r2);
	} while (--icount);

	backpatch2 = (u32 *)recMem;
	B(0); // b label_exit
	NOP();

	PC = pc - 4;

	regPopState();

	// label_hle:
	fixup_branch(backpatch1);
	fixup_branch(backpatch3);
#endif

	do {
		u32 opcode = *(u32 *)((char *)PSXM(PC));
		u32 insn = opcode & 0xfc000000;
		u32 rt = _fRt_(opcode);
		s32 imm = _fImm_(opcode);
		u32 r2 = regMipsToHost(rt, REG_LOAD, REG_REGISTER);

		ADDIU(MIPSREG_A0, r1, imm); // a0 = r1 & ~3
		INS(MIPSREG_A0, 0, 0, 2); // clear 2 lower bits
		CALLFunc((u32)psxMemRead32); // result in MIPSREG_V0

		ADDIU(MIPSREG_A0, r1, imm); // a0 = r1 & ~3
		INS(MIPSREG_A0, 0, 0, 2); // clear 2 lower bits

		ADDIU(TEMP_1, r1, imm);
		ANDI(TEMP_1, TEMP_1, 3); // shift = addr & 3
		SLL(TEMP_1, TEMP_1, 2);

		if (insn == 0xa8000000) // SWL
			LI32(TEMP_2, (u32)SWL_MASK);
		else			// SWR
			LI32(TEMP_2, (u32)SWR_MASK);

		ADDU(TEMP_2, TEMP_2, TEMP_1);
		LW(TEMP_2, TEMP_2, 0);
		AND(MIPSREG_A1, MIPSREG_V0, TEMP_2);

		if (insn == 0xa8000000) // SWL
			LI32(TEMP_2, (u32)SWL_SHIFT);
		else			// SWR
			LI32(TEMP_2, (u32)SWR_SHIFT);

		ADDU(TEMP_2, TEMP_2, TEMP_1);
		LW(TEMP_2, TEMP_2, 0);

		if (insn == 0xa8000000) // SWL
			SRLV(TEMP_3, r2, TEMP_2);
		else			// SWR
			SLLV(TEMP_3, r2, TEMP_2);

		OR(MIPSREG_A1, MIPSREG_A1, TEMP_3);

		CALLFunc((u32)psxMemWrite32);

		PC += 4;

		regUnlock(r2);
	} while (--count);

#ifdef USE_DIRECT_MEM_ACCESS
	// label_exit:
	fixup_branch(backpatch2);
#endif

	pc = PC;

	regUnlock(r1);
}

/* Calculate number of lwl/lwr or swl/swr opcodes */
static int calc_wl_wr(u32 op1, u32 op2)
{
	int count = 0;
	u32 PC = pc;
	u32 opcode = psxRegs.code;
	u32 rs = _Rs_; // base reg

	imm_min = imm_max = _fImm_(opcode);

	/* If in delay slot, set count to 1 */
	if (branch)
		return 1;

	while ((_fOp_(opcode) == op1 || _fOp_(opcode) == op2) && (_fRs_(opcode) == rs)) {

		/* Update min and max immediate values */
		if (_fImm_(opcode) > imm_max) imm_max = _fImm_(opcode);
		if (_fImm_(opcode) < imm_min) imm_min = _fImm_(opcode);

		opcode = *(u32 *)((char *)PSXM(PC));
		PC += 4;
		count++;
	}

#ifdef LOG_WL_WR
	if (count) {
		printf("\nFOUND %d opcodes, min: %d, max: %d\n", count, imm_min, imm_max);
		u32 dpc = pc - 4;
		for (; dpc < PC - 4; dpc += 4)
			disasm_psx(dpc);
	}
#endif

	return count;
}

static void recLWL()
{
	int count = calc_wl_wr(0x22, 0x26);

	if (autobias) cycles_pending += count;

#ifdef USE_CONST_ADDRESSES
	if (IsConst(_Rs_)) {
		u32 addr = iRegs[_Rs_].r + imm_min;
		if ((addr & 0x1fffffff) < 0x800000) {
			u32 r2 = regMipsToHost(_Rs_, REG_LOAD, REG_REGISTER);
			u32 PC = pc - 4;

			#ifdef WITH_DISASM
			for (int i = 0; i < count-1; i++)
				DISASM_PSX(pc + i * 4);
			#endif

			if ((u32)psxM == 0x10000000) {
				LUI(TEMP_2, 0x1000);
				INS(TEMP_2, r2, 0, 0x15);
			} else {
				LW(TEMP_2, PERM_REG_1, off(psxM));
				EXT(TEMP_1, r2, 0, 0x15);
				ADDU(TEMP_2, TEMP_2, TEMP_1);
			}

			do {
				u32 opcode = *(u32 *)((char *)PSXM(PC));
				s32 imm = _fImm_(opcode);
				u32 rt = _fRt_(opcode);
				u32 r1 = regMipsToHost(rt, REG_LOAD, REG_REGISTER);

				OPCODE(opcode & 0xfc000000, r1, TEMP_2, imm);

				SetUndef(rt);
				regMipsChanged(rt);
				regUnlock(r1);
				PC += 4;
			} while (--count);

			pc = PC;
			regUnlock(r2);

			return;
		}
	}
#endif

	gen_LWL_LWR(count);
}

static void recLWR()
{
	if (autobias) cycles_pending += 1;

	gen_LWL_LWR(1);
}

static void recSWL()
{
	int count = calc_wl_wr(0x2a, 0x2e);

	if (autobias) cycles_pending += count;

#ifdef USE_CONST_ADDRESSES
	if (IsConst(_Rs_)) {
		u32 addr = iRegs[_Rs_].r + imm_min;
		if ((addr & 0x1fffffff) < 0x800000) {
			u32 r2 = regMipsToHost(_Rs_, REG_LOAD, REG_REGISTER);
			u32 PC = pc - 4;

			#ifdef WITH_DISASM
			for (int i = 0; i < count-1; i++)
				DISASM_PSX(pc + i * 4);
			#endif

			if ((u32)psxM == 0x10000000) {
				LUI(TEMP_2, 0x1000);
				INS(TEMP_2, r2, 0, 0x15);
			} else {
				LW(TEMP_2, PERM_REG_1, off(psxM));
				EXT(TEMP_1, r2, 0, 0x15);
				ADDU(TEMP_2, TEMP_2, TEMP_1);
			}

			do {
				u32 opcode = *(u32 *)((char *)PSXM(PC));
				s32 imm = _fImm_(opcode);
				u32 rt = _fRt_(opcode);
				u32 r1 = regMipsToHost(rt, REG_LOAD, REG_REGISTER);

				OPCODE(opcode & 0xfc000000, r1, TEMP_2, imm);

				regUnlock(r1);
				PC += 4;
			} while (--count);

			pc = PC;
			regUnlock(r2);
			return;
		}
	}
#endif

	gen_SWL_SWR(count);
}

static void recSWR()
{
	if (autobias) cycles_pending += 1;

	gen_SWL_SWR(1);
}
