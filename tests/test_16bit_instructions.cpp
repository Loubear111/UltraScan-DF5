#include "test_helpers.h"

// ============================================================
// 16-bit ("!") instruction set. These are routed through decode16()
// because their encoders leave bit 31 clear. GPR fields address
// r0..r15; the high bank (r16..r31) is reached via the H bit
// (pop/push) or the cross-bank moves (mlfh/mhfl).
//
// Per spec the ALU "!" ops leave N/Z/C/V undefined, so these tests
// only assert the register results for those; cmp!/bittst!/t{cond}!
// have defined flag behavior and are checked accordingly.
// ============================================================

// ---- RR-ALU forms (op16 0x0) ----
TEST_F(CPUFixture, ADD16_Basic)
{
    setReg(4, 5u);
    setReg(2, 3u);
    execute16(encodeADD16(4, 2));
    EXPECT_EQ(getReg(4), 8u);
}

TEST_F(CPUFixture, ADDC16_AddsCarry)
{
    bus.cpu.status |= spg290::C;
    setReg(4, 5u);
    setReg(2, 3u);
    execute16(encodeADDC16(4, 2));
    EXPECT_EQ(getReg(4), 9u);   // 5 + 3 + 1
}

TEST_F(CPUFixture, SUB16_Basic)
{
    setReg(4, 10u);
    setReg(2, 4u);
    execute16(encodeSUB16(4, 2));
    EXPECT_EQ(getReg(4), 6u);
}

TEST_F(CPUFixture, AND16_Basic)
{
    setReg(4, 0xF0F0u);
    setReg(2, 0xFF00u);
    execute16(encodeAND16(4, 2));
    EXPECT_EQ(getReg(4), 0xF000u);
}

TEST_F(CPUFixture, OR16_Basic)
{
    setReg(4, 0x0F0Fu);
    setReg(2, 0xF000u);
    execute16(encodeOR16(4, 2));
    EXPECT_EQ(getReg(4), 0xFF0Fu);
}

TEST_F(CPUFixture, XOR16_Basic)
{
    setReg(4, 0xFFFFu);
    setReg(2, 0x0F0Fu);
    execute16(encodeXOR16(4, 2));
    EXPECT_EQ(getReg(4), 0xF0F0u);
}

TEST_F(CPUFixture, MV16_CopiesRegister)
{
    setReg(4, 0u);
    setReg(2, 0x12345678u);
    execute16(encodeMV16(4, 2));
    EXPECT_EQ(getReg(4), 0x12345678u);
}

TEST_F(CPUFixture, NEG16_Negates)
{
    setReg(2, 5u);
    execute16(encodeNEG16(4, 2));
    EXPECT_EQ(getReg(4), 0xFFFFFFFBu);  // -5
}

TEST_F(CPUFixture, NOT16_Complements)
{
    setReg(2, 0u);
    execute16(encodeNOT16(4, 2));
    EXPECT_EQ(getReg(4), 0xFFFFFFFFu);
}

TEST_F(CPUFixture, CMP16_EqualSetsZeroAndCarry)
{
    setReg(4, 5u);
    setReg(2, 5u);
    execute16(encodeCMP16(4, 2));
    EXPECT_EQ(getFlag(spg290::Z), 1);
    EXPECT_EQ(getFlag(spg290::C), 1);   // ~borrow
    EXPECT_EQ(getFlag(spg290::N), 0);
}

TEST_F(CPUFixture, CMP16_LessThanSetsNegativeNoCarry)
{
    setReg(4, 3u);
    setReg(2, 5u);
    execute16(encodeCMP16(4, 2));         // 3 - 5
    EXPECT_EQ(getFlag(spg290::Z), 0);
    EXPECT_EQ(getFlag(spg290::C), 0);   // borrow occurred
    EXPECT_EQ(getFlag(spg290::N), 1);
}

TEST_F(CPUFixture, SLL16_ShiftsByRegister)
{
    setReg(4, 1u);
    setReg(2, 4u);
    execute16(encodeSLL16(4, 2));
    EXPECT_EQ(getReg(4), 0x10u);
}

TEST_F(CPUFixture, SRL16_ShiftsByRegister)
{
    setReg(4, 0x10u);
    setReg(2, 4u);
    execute16(encodeSRL16(4, 2));
    EXPECT_EQ(getReg(4), 1u);
}

TEST_F(CPUFixture, SRA16_SignExtends)
{
    setReg(4, 0x80000000u);
    setReg(2, 4u);
    execute16(encodeSRA16(4, 2));
    EXPECT_EQ(getReg(4), 0xF8000000u);
}

// ---- ldiu! (op16 0x1) ----
TEST_F(CPUFixture, LDIU16_ZeroExtendsImm8)
{
    setReg(4, 0xFFFFFFFFu);
    execute16(encodeLDIU16(4, 0xE));
    EXPECT_EQ(getReg(4), 0x0000000Eu);
}

// ---- R-Imm forms (op16 0x2) ----
TEST_F(CPUFixture, SLLI16_Basic)
{
    setReg(4, 1u);
    execute16(encodeSLLI16(4, 4));
    EXPECT_EQ(getReg(4), 0x10u);
}

TEST_F(CPUFixture, SRLI16_Basic)
{
    setReg(4, 0x10u);
    execute16(encodeSRLI16(4, 4));
    EXPECT_EQ(getReg(4), 1u);
}

TEST_F(CPUFixture, ADDEI16_AddsPowerOfTwo)
{
    setReg(4, 10u);
    execute16(encodeADDEI16(4, 3));   // + 2^3 = 8
    EXPECT_EQ(getReg(4), 18u);
}

TEST_F(CPUFixture, SUBEI16_SubtractsPowerOfTwo)
{
    setReg(4, 20u);
    execute16(encodeSUBEI16(4, 2));   // - 2^2 = 4
    EXPECT_EQ(getReg(4), 16u);
}

// ---- bittst! (op16 0x3) ----
TEST_F(CPUFixture, BITTST16_SetBitClearsZeroSetsNegative)
{
    setReg(4, 0x80000000u);
    execute16(encodeBITTST16(4, 31));
    EXPECT_EQ(getFlag(spg290::Z), 0);   // bit 31 is set
    EXPECT_EQ(getFlag(spg290::N), 1);   // N = rD[31]
}

TEST_F(CPUFixture, BITTST16_ClearBitSetsZero)
{
    setReg(4, 0x80000000u);
    execute16(encodeBITTST16(4, 0));
    EXPECT_EQ(getFlag(spg290::Z), 1);   // bit 0 is clear
    EXPECT_EQ(getFlag(spg290::N), 1);
}

// ---- cross-bank moves (op16 0x4) ----
TEST_F(CPUFixture, MLFH16_MovesFromHighBank)
{
    setReg(20, 0xCAFEBABEu);            // r20 is in the high bank
    execute16(encodeMLFH16(4, 4));        // r4(lo) = r(4+16)=r20
    EXPECT_EQ(getReg(4), 0xCAFEBABEu);
}

TEST_F(CPUFixture, MHFL16_MovesToHighBank)
{
    setReg(5, 0xDEADBEEFu);
    execute16(encodeMHFL16(4, 5));        // r(4+16)=r20 = r5(lo)
    EXPECT_EQ(getReg(20), 0xDEADBEEFu);
}

// ---- memory simple (op16 0x5) ----
TEST_F(CPUFixture, LW16_Loads)
{
    setReg(6, 50u);                     // base address
    setMem(50,  0x11223344u);
    execute16(encodeLW16(4, 6));
    EXPECT_EQ(getReg(4), 0x11223344u);
}

TEST_F(CPUFixture, LH16_SignExtends)
{
    setReg(6, 50u);
    setMem(50,  0xFFFF0000u);
    execute16(encodeLH16(4, 6));
    EXPECT_EQ(getReg(4), 0xFFFFFFFFu);
}

TEST_F(CPUFixture, LBU16_ZeroExtends)
{
    setReg(6, 50u);
    setMem(50,  0xDEADBEEFu);
    execute16(encodeLBU16(4, 6));
    EXPECT_EQ(getReg(4), 0x000000DEu);
}

TEST_F(CPUFixture, SW16_Stores)
{
    setReg(4, 0xABCDEF01u);
    setReg(6, 60u);
    execute16(encodeSW16(4, 6));
    EXPECT_EQ(getMem(60), 0xABCDEF01u);
}

TEST_F(CPUFixture, SB16_PreservesUpperBytes)
{
    setReg(4, 0x000000ABu);
    setReg(6, 60u);
    setMem(60,  0x11223344u);
    execute16(encodeSB16(4, 6));
    EXPECT_EQ(getMem(60), 0xAB223344u);
}

TEST_F(CPUFixture, SH16_PreservesUpperHalf)
{
    setReg(4, 0x0000ABCDu);
    setReg(6, 60u);
    setMem(60,  0x11223344u);
    execute16(encodeSH16(4, 6));
    EXPECT_EQ(getMem(60), 0xABCD3344u);
}

// ---- memory BP-relative (op16 0x6, BP = r2) ----
TEST_F(CPUFixture, LWP16_LoadsFromBasePointer)
{
    setReg(2, 40u);                     // BP
    setMem(44,  0x55667788u);            // 40 + (1<<2)
    execute16(encodeLWP16(4, 1));
    EXPECT_EQ(getReg(4), 0x55667788u);
}

TEST_F(CPUFixture, LBUP16_LoadsByteFromBasePointer)
{
    setReg(2, 40u);
    setMem(45,  0xA5000000u);            // 40 + 5
    execute16(encodeLBUP16(4, 5));
    EXPECT_EQ(getReg(4), 0x000000A5u);
}

TEST_F(CPUFixture, SWP16_StoresToBasePointer)
{
    setReg(2, 40u);
    setReg(4, 0x99AABBCCu);
    execute16(encodeSWP16(4, 2));         // addr = 40 + (2<<2) = 48
    EXPECT_EQ(getMem(48), 0x99AABBCCu);
}

// ---- stack (op16 0x7) ----
TEST_F(CPUFixture, PUSH16_PreDecrementsAndStores)
{
    setReg(5, 0x12121212u);
    setReg(3, 100u);                    // stack pointer base
    execute16(encodePUSH16(5, 3, /*high=*/false));
    EXPECT_EQ(getMem(96), 0x12121212u); // stored at base-4
    EXPECT_EQ(getReg(3), 96u);          // base decremented
}

TEST_F(CPUFixture, POP16_LoadsAndPostIncrements)
{
    setReg(3, 96u);                     // stack pointer
    setMem(96,  0x34343434u);
    execute16(encodePOP16(5, 3, /*high=*/false));
    EXPECT_EQ(getReg(5), 0x34343434u);
    EXPECT_EQ(getReg(3), 100u);         // base incremented by 4
}

TEST_F(CPUFixture, PUSHPOP16_HighBankRoundTrip)
{
    setReg(18, 0x77777777u);            // high-bank source
    setReg(3, 100u);
    execute16(encodePUSH16(2, 3, /*high=*/true));  // push r(2+16)=r18
    EXPECT_EQ(getMem(96), 0x77777777u);
    execute16(encodePOP16(4, 3, /*high=*/true));   // pop into r(4+16)=r20
    EXPECT_EQ(getReg(20), 0x77777777u);
    EXPECT_EQ(getReg(3), 100u);
}

// ---- branches / jump (op16 0x7, sub 0x1/0x2/0x3) ----
// pc is a byte address; a 16-bit instruction is 2 bytes (instrSize=2) and
// branch/jump displacements are halfword-granular (disp<<1), matching the
// 32-bit BCOND/JX convention (see src/spg290.cpp).
TEST_F(CPUFixture, BCOND16_TakenWhenEQ)
{
    bus.cpu.status |= spg290::Z;
    execute16(encodeBCOND16(4, 5));       // beq! +5 halfwords, executed at pc=0
    EXPECT_EQ(bus.cpu.pc, 10u);
}

TEST_F(CPUFixture, BCOND16_NotTaken)
{
    execute16(encodeBCOND16(4, 5));       // Z clear
    EXPECT_EQ(bus.cpu.pc, 2u);
}

TEST_F(CPUFixture, BCOND16_NegativeDisplacement)
{
    bus.cpu.pc = 10;
    bus.cpu.status |= spg290::Z;
    execute16(encodeBCOND16(4, -5));
    EXPECT_EQ(bus.cpu.pc, 0u);
}

TEST_F(CPUFixture, BRCOND16_TakenToRegister)
{
    setReg(5, 42u);
    bus.cpu.status |= spg290::Z;
    execute16(encodeBRCOND16(4, 5));
    EXPECT_EQ(bus.cpu.pc, 42u);
}

TEST_F(CPUFixture, JX16_Jump)
{
    execute16(encodeJX16(20, false));
    EXPECT_EQ(bus.cpu.pc, 40u);
}

TEST_F(CPUFixture, JX16_JumpAndLink)
{
    execute16(encodeJX16(20, true));
    EXPECT_EQ(bus.cpu.pc, 40u);
    EXPECT_EQ(getReg(3), 2u);           // link = following instruction
}

// ---- system (op16 0x7, sub 0x4) ----
TEST_F(CPUFixture, NOP16_DoesNothing)
{
    setReg(4, 0x12345678u);
    execute16(encodeNOP16());
    EXPECT_EQ(getReg(4), 0x12345678u);
    EXPECT_EQ(bus.cpu.pc, 2u);
}

TEST_F(CPUFixture, SDBBP16_TreatedAsNoOp)
{
    setReg(4, 0xAAAAAAAAu);
    execute16(encodeSDBBP16());
    EXPECT_EQ(getReg(4), 0xAAAAAAAAu);
}

TEST_F(CPUFixture, TCOND16_SetsTrueWhenConditionHolds)
{
    bus.cpu.status |= spg290::Z;        // EQ holds
    execute16(encodeTCOND16(4));
    EXPECT_EQ(getFlag(spg290::T), 1);
}

TEST_F(CPUFixture, TCOND16_ClearsWhenConditionFails)
{
    bus.cpu.status |= spg290::T;        // start set
    execute16(encodeTCOND16(4));          // EQ, but Z clear -> condition false
    EXPECT_EQ(getFlag(spg290::T), 0);
}

// ============================================================
// Regression: a 16-bit instruction is 2 bytes and a 32-bit instruction is
// 4 bytes, packed at their own natural size in byte-addressed memory (not
// one fixed-width slot per instruction). Place one of each back-to-back
// and step the CPU instruction-by-instruction to prove pc advances by the
// real size of whichever form was just fetched.
// ============================================================
TEST_F(CPUFixture, MixedWidthInstructions_FetchAdvancesByRealSize)
{
    setReg(4, 10u);
    setReg(5, 5u);
    bus.write16(0, (uint16_t)(encodeADD16(4, 5) >> 16));   // add! r4, r5 (2 bytes @ 0)
    bus.write32(2, encodeADDIX(6, 1, false));              // addi r6, 1 (4 bytes @ 2)

    bus.cpu.clock();                 // executes the 16-bit add!
    EXPECT_EQ(bus.cpu.pc, 2u);
    EXPECT_EQ(getReg(4), 15u);

    bus.cpu.clock();                 // executes the 32-bit addi
    EXPECT_EQ(bus.cpu.pc, 6u);
    EXPECT_EQ(getReg(6), 1u);
}


// ---- Story 6a: OP 0 / OP 2 flag effects, brl!, undefined encodings ----
// The "!" ALU ops are CU = 1: they update the flags like the 32-bit .c forms.
TEST_F(CPUFixture, ADD16_UpdatesCarryAndOverflow)
{
    setReg(4, 0x7FFFFFFFu);
    setReg(2, 1u);
    execute16(encodeADD16(4, 2));
    EXPECT_EQ(getReg(4), 0x80000000u);
    EXPECT_EQ(getFlag(spg290::N), 1);
    EXPECT_EQ(getFlag(spg290::V), 1);
    EXPECT_EQ(getFlag(spg290::C), 0);

    bus.cpu.pc = 0x100;
    setReg(4, 0xFFFFFFFFu);
    execute16(encodeADD16(4, 2));
    EXPECT_EQ(getReg(4), 0u);
    EXPECT_EQ(getFlag(spg290::Z), 1);
    EXPECT_EQ(getFlag(spg290::C), 1);
}

TEST_F(CPUFixture, SUB16_UpdatesFlagsAsNoBorrow)
{
    setReg(4, 3u);
    setReg(2, 5u);
    execute16(encodeSUB16(4, 2));
    EXPECT_EQ(getReg(4), 0xFFFFFFFEu);
    EXPECT_EQ(getFlag(spg290::N), 1);
    EXPECT_EQ(getFlag(spg290::C), 0);   // borrow
}

TEST_F(CPUFixture, NEG16_OfZeroSetsZeroAndCarry)
{
    setReg(1, 0u);
    execute16(encodeNEG16(3, 1));
    EXPECT_EQ(getReg(3), 0u);
    EXPECT_EQ(getFlag(spg290::Z), 1);
    EXPECT_EQ(getFlag(spg290::C), 1);
}

TEST_F(CPUFixture, AND16_SetsNegativeAndZero)
{
    setReg(4, 0x80000000u);
    setReg(2, 0xC0000000u);
    execute16(encodeAND16(4, 2));
    EXPECT_EQ(getFlag(spg290::N), 1);
    EXPECT_EQ(getFlag(spg290::Z), 0);
}

TEST_F(CPUFixture, SLL16_CarryIsLastBitShiftedOut)
{
    setReg(4, 0x80000001u);
    setReg(2, 1u);
    execute16(encodeSLL16(4, 2));
    EXPECT_EQ(getReg(4), 2u);
    EXPECT_EQ(getFlag(spg290::C), 1);
}

TEST_F(CPUFixture, SRA16_UsesLowFiveBitsOfRa)
{
    setReg(4, 0x80000000u);
    setReg(2, 0x21u);                   // 33 & 0x1F = 1
    execute16(encodeSRA16(4, 2));
    EXPECT_EQ(getReg(4), 0xC0000000u);
}

TEST_F(CPUFixture, BRCOND16_NotTakenFallsThrough)
{
    bus.cpu.pc = 0x100;
    setReg(5, 0x2000u);
    bus.cpu.status &= ~spg290::Z;
    execute16(encodeBRCOND16(4 /*eq*/, 5));
    EXPECT_EQ(bus.cpu.pc, 0x102u);
}

TEST_F(CPUFixture, BRLCOND16_LinksEvenWhenNotTaken)
{
    bus.cpu.pc = 0x100;
    setReg(5, 0x2000u);
    bus.cpu.status &= ~spg290::Z;
    execute16(encodeBRLCOND16(4 /*eq*/, 5));
    EXPECT_EQ(bus.cpu.pc, 0x102u);
    EXPECT_EQ(getReg(3), 0x102u);
}

TEST_F(CPUFixture, BRLCOND16_TakenJumpsAndLinksNextPc)
{
    bus.cpu.pc = 0x100;
    setReg(5, 0x2000u);
    bus.cpu.status |= spg290::Z;
    execute16(encodeBRLCOND16(4 /*eq*/, 5));
    EXPECT_EQ(bus.cpu.pc, 0x2000u);
    EXPECT_EQ(getReg(3), 0x102u);
}

TEST_F(CPUFixture, BR16_AlwaysIsReturnThroughR3)
{
    bus.cpu.pc = 0x100;
    setReg(3, 0x4000u);
    execute16(encodeBRCOND16(15, 3));   // br! r3
    EXPECT_EQ(bus.cpu.pc, 0x4000u);
}

TEST_F(CPUFixture, Op0UndefinedFuncRaisesReservedInstruction)
{
    bus.cpu.pc = 0x100;
    execute16(enc16_R(0, 1, 2, 0x6));   // func4 6 is undefined in OP 0
    EXPECT_EQ(bus.cpu.cr[2] & spg290::ECR_CODE_MASK, 9u);   // RI
    EXPECT_EQ(bus.cpu.cr[5], 0x100u);
}


// ---- Story 6b: OP 3-7 ----
TEST_F(CPUFixture, JX16_KeepsUpperPcBitsOfCurrentPage)
{
    bus.cpu.pc = 0x00012340;
    execute16(encodeJX16(0x123, false));        // Disp11 << 1 = 0x246
    EXPECT_EQ(bus.cpu.pc, 0x00012246u);         // PC[31:12] kept, PC[11:0] replaced
}

TEST_F(CPUFixture, JL16_LinksCurrentPcPlusTwo)
{
    bus.cpu.pc = 0x00012340;
    execute16(encodeJX16(0x10, true));
    EXPECT_EQ(getReg(3), 0x00012342u);
    EXPECT_EQ(bus.cpu.pc, 0x00012020u);
}

TEST_F(CPUFixture, B16_BackwardToSelfIsFourF00)
{
    EXPECT_EQ(encodeB16(15, 0), 0x4F00);
    bus.cpu.pc = 0x100;
    execute16(encodeB16(15, 0));                // disp 0 = branch to itself
    EXPECT_EQ(bus.cpu.pc, 0x100u);
}

TEST_F(CPUFixture, LDIU16_ZeroExtendsHighBitImmediate)
{
    execute16(encodeLDIU16(9, 0xFF));
    EXPECT_EQ(getReg(9), 0xFFu);
}

TEST_F(CPUFixture, Op1IsUndefinedAndRaisesReservedInstruction)
{
    bus.cpu.pc = 0x100;
    execute16(0x1000);
    EXPECT_EQ(bus.cpu.cr[2] & spg290::ECR_CODE_MASK, 9u);
    EXPECT_EQ(bus.cpu.cr[5], 0x100u);
}

TEST_F(CPUFixture, ADDEI16_UpdatesFlags)
{
    setReg(4, 0xFFFFFFFFu);
    execute16(encodeADDEI16(4, 0));             // + 1
    EXPECT_EQ(getReg(4), 0u);
    EXPECT_EQ(getFlag(spg290::Z), 1);
    EXPECT_EQ(getFlag(spg290::C), 1);
}

TEST_F(CPUFixture, SLLI16_CarryIsLastBitOut)
{
    setReg(4, 0x80000000u);
    execute16(encodeSLLI16(4, 1));
    EXPECT_EQ(getReg(4), 0u);
    EXPECT_EQ(getFlag(spg290::C), 1);
    EXPECT_EQ(getFlag(spg290::Z), 1);
}

TEST_F(CPUFixture, BITCLR_BITSET_BITTGL16_ChangeOneBit)
{
    setReg(4, 0xFFu);
    execute16(encodeBITCLR16(4, 3));
    EXPECT_EQ(getReg(4), 0xF7u);

    bus.cpu.pc = 0x100;
    execute16(encodeBITSET16(4, 8));
    EXPECT_EQ(getReg(4), 0x1F7u);

    bus.cpu.pc = 0x100;
    execute16(encodeBITTGL16(4, 0));
    EXPECT_EQ(getReg(4), 0x1F6u);
}

TEST_F(CPUFixture, LHP16_ScalesByTwoAndSignExtends)
{
    setReg(2, 0x200u);
    bus.write16(0x200 + 2 * 3, 0x8001);
    execute16(encodeLHP16(5, 3));
    EXPECT_EQ(getReg(5), 0xFFFF8001u);
}

TEST_F(CPUFixture, SHP16_And_SBP16_StoreAtBpOffsets)
{
    setReg(2, 0x200u);
    setReg(5, 0x12345678u);
    execute16(encodeSHP16(5, 2));               // [BP + 4]
    EXPECT_EQ(bus.read16(0x204), 0x5678u);

    bus.cpu.pc = 0x100;
    execute16(encodeSBP16(5, 7));               // [BP + 7]
    EXPECT_EQ(bus.read8(0x207), 0x78u);
}

TEST_F(CPUFixture, Op7UndefinedFuncRaisesReservedInstruction)
{
    bus.cpu.pc = 0x100;
    execute16(enc16_I1(7, 1, 0, 2));
    EXPECT_EQ(bus.cpu.cr[2] & spg290::ECR_CODE_MASK, 9u);
}
