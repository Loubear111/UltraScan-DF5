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
    execute(encodeADD16(4, 2));
    EXPECT_EQ(getReg(4), 8u);
}

TEST_F(CPUFixture, ADDC16_AddsCarry)
{
    bus.cpu.status |= spg290::C;
    setReg(4, 5u);
    setReg(2, 3u);
    execute(encodeADDC16(4, 2));
    EXPECT_EQ(getReg(4), 9u);   // 5 + 3 + 1
}

TEST_F(CPUFixture, SUB16_Basic)
{
    setReg(4, 10u);
    setReg(2, 4u);
    execute(encodeSUB16(4, 2));
    EXPECT_EQ(getReg(4), 6u);
}

TEST_F(CPUFixture, AND16_Basic)
{
    setReg(4, 0xF0F0u);
    setReg(2, 0xFF00u);
    execute(encodeAND16(4, 2));
    EXPECT_EQ(getReg(4), 0xF000u);
}

TEST_F(CPUFixture, OR16_Basic)
{
    setReg(4, 0x0F0Fu);
    setReg(2, 0xF000u);
    execute(encodeOR16(4, 2));
    EXPECT_EQ(getReg(4), 0xFF0Fu);
}

TEST_F(CPUFixture, XOR16_Basic)
{
    setReg(4, 0xFFFFu);
    setReg(2, 0x0F0Fu);
    execute(encodeXOR16(4, 2));
    EXPECT_EQ(getReg(4), 0xF0F0u);
}

TEST_F(CPUFixture, MV16_CopiesRegister)
{
    setReg(4, 0u);
    setReg(2, 0x12345678u);
    execute(encodeMV16(4, 2));
    EXPECT_EQ(getReg(4), 0x12345678u);
}

TEST_F(CPUFixture, NEG16_Negates)
{
    setReg(2, 5u);
    execute(encodeNEG16(4, 2));
    EXPECT_EQ(getReg(4), 0xFFFFFFFBu);  // -5
}

TEST_F(CPUFixture, NOT16_Complements)
{
    setReg(2, 0u);
    execute(encodeNOT16(4, 2));
    EXPECT_EQ(getReg(4), 0xFFFFFFFFu);
}

TEST_F(CPUFixture, CMP16_EqualSetsZeroAndCarry)
{
    setReg(4, 5u);
    setReg(2, 5u);
    execute(encodeCMP16(4, 2));
    EXPECT_EQ(getFlag(spg290::Z), 1);
    EXPECT_EQ(getFlag(spg290::C), 1);   // ~borrow
    EXPECT_EQ(getFlag(spg290::N), 0);
}

TEST_F(CPUFixture, CMP16_LessThanSetsNegativeNoCarry)
{
    setReg(4, 3u);
    setReg(2, 5u);
    execute(encodeCMP16(4, 2));         // 3 - 5
    EXPECT_EQ(getFlag(spg290::Z), 0);
    EXPECT_EQ(getFlag(spg290::C), 0);   // borrow occurred
    EXPECT_EQ(getFlag(spg290::N), 1);
}

TEST_F(CPUFixture, SLL16_ShiftsByRegister)
{
    setReg(4, 1u);
    setReg(2, 4u);
    execute(encodeSLL16(4, 2));
    EXPECT_EQ(getReg(4), 0x10u);
}

TEST_F(CPUFixture, SRL16_ShiftsByRegister)
{
    setReg(4, 0x10u);
    setReg(2, 4u);
    execute(encodeSRL16(4, 2));
    EXPECT_EQ(getReg(4), 1u);
}

TEST_F(CPUFixture, SRA16_SignExtends)
{
    setReg(4, 0x80000000u);
    setReg(2, 4u);
    execute(encodeSRA16(4, 2));
    EXPECT_EQ(getReg(4), 0xF8000000u);
}

// ---- ldiu! (op16 0x1) ----
TEST_F(CPUFixture, LDIU16_ZeroExtendsImm8)
{
    setReg(4, 0xFFFFFFFFu);
    execute(encodeLDIU16(4, 0xE));
    EXPECT_EQ(getReg(4), 0x0000000Eu);
}

// ---- R-Imm forms (op16 0x2) ----
TEST_F(CPUFixture, SLLI16_Basic)
{
    setReg(4, 1u);
    execute(encodeSLLI16(4, 4));
    EXPECT_EQ(getReg(4), 0x10u);
}

TEST_F(CPUFixture, SRLI16_Basic)
{
    setReg(4, 0x10u);
    execute(encodeSRLI16(4, 4));
    EXPECT_EQ(getReg(4), 1u);
}

TEST_F(CPUFixture, ADDEI16_AddsPowerOfTwo)
{
    setReg(4, 10u);
    execute(encodeADDEI16(4, 3));   // + 2^3 = 8
    EXPECT_EQ(getReg(4), 18u);
}

TEST_F(CPUFixture, SUBEI16_SubtractsPowerOfTwo)
{
    setReg(4, 20u);
    execute(encodeSUBEI16(4, 2));   // - 2^2 = 4
    EXPECT_EQ(getReg(4), 16u);
}

// ---- bittst! (op16 0x3) ----
TEST_F(CPUFixture, BITTST16_SetBitClearsZeroSetsNegative)
{
    setReg(4, 0x80000000u);
    execute(encodeBITTST16(4, 31));
    EXPECT_EQ(getFlag(spg290::Z), 0);   // bit 31 is set
    EXPECT_EQ(getFlag(spg290::N), 1);   // N = rD[31]
}

TEST_F(CPUFixture, BITTST16_ClearBitSetsZero)
{
    setReg(4, 0x80000000u);
    execute(encodeBITTST16(4, 0));
    EXPECT_EQ(getFlag(spg290::Z), 1);   // bit 0 is clear
    EXPECT_EQ(getFlag(spg290::N), 1);
}

// ---- cross-bank moves (op16 0x4) ----
TEST_F(CPUFixture, MLFH16_MovesFromHighBank)
{
    setReg(20, 0xCAFEBABEu);            // r20 is in the high bank
    execute(encodeMLFH16(4, 4));        // r4(lo) = r(4+16)=r20
    EXPECT_EQ(getReg(4), 0xCAFEBABEu);
}

TEST_F(CPUFixture, MHFL16_MovesToHighBank)
{
    setReg(5, 0xDEADBEEFu);
    execute(encodeMHFL16(4, 5));        // r(4+16)=r20 = r5(lo)
    EXPECT_EQ(getReg(20), 0xDEADBEEFu);
}

// ---- memory simple (op16 0x5) ----
TEST_F(CPUFixture, LW16_Loads)
{
    setReg(6, 50u);                     // base address
    setReg(50, 0x11223344u);
    execute(encodeLW16(4, 6));
    EXPECT_EQ(getReg(4), 0x11223344u);
}

TEST_F(CPUFixture, LH16_SignExtends)
{
    setReg(6, 50u);
    setReg(50, 0x0000FFFFu);
    execute(encodeLH16(4, 6));
    EXPECT_EQ(getReg(4), 0xFFFFFFFFu);
}

TEST_F(CPUFixture, LBU16_ZeroExtends)
{
    setReg(6, 50u);
    setReg(50, 0xDEADBEEFu);
    execute(encodeLBU16(4, 6));
    EXPECT_EQ(getReg(4), 0x000000EFu);
}

TEST_F(CPUFixture, SW16_Stores)
{
    setReg(4, 0xABCDEF01u);
    setReg(6, 60u);
    execute(encodeSW16(4, 6));
    EXPECT_EQ(getReg(60), 0xABCDEF01u);
}

TEST_F(CPUFixture, SB16_PreservesUpperBytes)
{
    setReg(4, 0x000000ABu);
    setReg(6, 60u);
    setReg(60, 0x11223344u);
    execute(encodeSB16(4, 6));
    EXPECT_EQ(getReg(60), 0x112233ABu);
}

TEST_F(CPUFixture, SH16_PreservesUpperHalf)
{
    setReg(4, 0x0000ABCDu);
    setReg(6, 60u);
    setReg(60, 0x11223344u);
    execute(encodeSH16(4, 6));
    EXPECT_EQ(getReg(60), 0x1122ABCDu);
}

// ---- memory BP-relative (op16 0x6, BP = r2) ----
TEST_F(CPUFixture, LWP16_LoadsFromBasePointer)
{
    setReg(2, 40u);                     // BP
    setReg(44, 0x55667788u);            // 40 + (1<<2)
    execute(encodeLWP16(4, 1));
    EXPECT_EQ(getReg(4), 0x55667788u);
}

TEST_F(CPUFixture, LBUP16_LoadsByteFromBasePointer)
{
    setReg(2, 40u);
    setReg(45, 0x000000A5u);            // 40 + 5
    execute(encodeLBUP16(4, 5));
    EXPECT_EQ(getReg(4), 0x000000A5u);
}

TEST_F(CPUFixture, SWP16_StoresToBasePointer)
{
    setReg(2, 40u);
    setReg(4, 0x99AABBCCu);
    execute(encodeSWP16(4, 2));         // addr = 40 + (2<<2) = 48
    EXPECT_EQ(getReg(48), 0x99AABBCCu);
}

// ---- stack (op16 0x7) ----
TEST_F(CPUFixture, PUSH16_PreDecrementsAndStores)
{
    setReg(5, 0x12121212u);
    setReg(3, 100u);                    // stack pointer base
    execute(encodePUSH16(5, 3, /*high=*/false));
    EXPECT_EQ(getReg(96), 0x12121212u); // stored at base-4
    EXPECT_EQ(getReg(3), 96u);          // base decremented
}

TEST_F(CPUFixture, POP16_LoadsAndPostIncrements)
{
    setReg(3, 96u);                     // stack pointer
    setReg(96, 0x34343434u);
    execute(encodePOP16(5, 3, /*high=*/false));
    EXPECT_EQ(getReg(5), 0x34343434u);
    EXPECT_EQ(getReg(3), 100u);         // base incremented by 4
}

TEST_F(CPUFixture, PUSHPOP16_HighBankRoundTrip)
{
    setReg(18, 0x77777777u);            // high-bank source
    setReg(3, 100u);
    execute(encodePUSH16(2, 3, /*high=*/true));  // push r(2+16)=r18
    EXPECT_EQ(getReg(96), 0x77777777u);
    execute(encodePOP16(4, 3, /*high=*/true));   // pop into r(4+16)=r20
    EXPECT_EQ(getReg(20), 0x77777777u);
    EXPECT_EQ(getReg(3), 100u);
}

// ---- branches / jump (op16 0x8 / 0x9 / 0xA) ----
TEST_F(CPUFixture, BCOND16_TakenWhenEQ)
{
    bus.cpu.status |= spg290::Z;
    execute(encodeBCOND16(4, 5));       // beq! +5
    EXPECT_EQ(bus.cpu.pc, 5u);
}

TEST_F(CPUFixture, BCOND16_NotTaken)
{
    execute(encodeBCOND16(4, 5));       // Z clear
    EXPECT_EQ(bus.cpu.pc, 1u);
}

TEST_F(CPUFixture, BCOND16_NegativeDisplacement)
{
    bus.cpu.pc = 10;
    bus.cpu.status |= spg290::Z;
    execute(encodeBCOND16(4, -5));
    EXPECT_EQ(bus.cpu.pc, 5u);
}

TEST_F(CPUFixture, BRCOND16_TakenToRegister)
{
    setReg(5, 42u);
    bus.cpu.status |= spg290::Z;
    execute(encodeBRCOND16(4, 5));
    EXPECT_EQ(bus.cpu.pc, 42u);
}

TEST_F(CPUFixture, JX16_Jump)
{
    execute(encodeJX16(20, false));
    EXPECT_EQ(bus.cpu.pc, 20u);
}

TEST_F(CPUFixture, JX16_JumpAndLink)
{
    execute(encodeJX16(20, true));
    EXPECT_EQ(bus.cpu.pc, 20u);
    EXPECT_EQ(getReg(3), 1u);           // link = following instruction
}

// ---- system (op16 0xB) ----
TEST_F(CPUFixture, NOP16_DoesNothing)
{
    setReg(4, 0x12345678u);
    execute(encodeNOP16());
    EXPECT_EQ(getReg(4), 0x12345678u);
    EXPECT_EQ(bus.cpu.pc, 1u);
}

TEST_F(CPUFixture, SDBBP16_TreatedAsNoOp)
{
    setReg(4, 0xAAAAAAAAu);
    execute(encodeSDBBP16());
    EXPECT_EQ(getReg(4), 0xAAAAAAAAu);
}

TEST_F(CPUFixture, TCOND16_SetsTrueWhenConditionHolds)
{
    bus.cpu.status |= spg290::Z;        // EQ holds
    execute(encodeTCOND16(4));
    EXPECT_EQ(getFlag(spg290::T), 1);
}

TEST_F(CPUFixture, TCOND16_ClearsWhenConditionFails)
{
    bus.cpu.status |= spg290::T;        // start set
    execute(encodeTCOND16(4));          // EQ, but Z clear -> condition false
    EXPECT_EQ(getFlag(spg290::T), 0);
}
