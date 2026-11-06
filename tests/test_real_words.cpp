// Behaviour of real 32-bit encodings (step E): words taken from the demos'
// disassembly plus the forms added by the table corrections. The encoders are
// checked against the disassembler in test_encoding_conformance.cpp.
#include "test_helpers.h"

TEST_F(CPUFixture, RealWord_SwPreIndex_StoresBelowAndUpdatesBase)
{
    setReg(0, 0x1000u);
    setReg(2, 0xAABBCCDDu);
    execute(0x8c40ffe4u);                       // sw r2, [r0, -4]+
    EXPECT_EQ(getMem(0xFFC), 0xAABBCCDDu);
    EXPECT_EQ(getReg(0), 0xFFCu);
    EXPECT_EQ(bus.cpu.pc, 4u);
}

TEST_F(CPUFixture, RealWord_SbPostIndex_StoresThenAdvancesBase)
{
    setReg(8, 0x2000u);
    setReg(10, 0x1234ABCDu);
    execute(0x9d48800fu);                       // sb r10, [r8]+, 1
    EXPECT_EQ(bus.read8(0x2000), 0xCDu);
    EXPECT_EQ(getReg(8), 0x2001u);
}

TEST_F(CPUFixture, RealWord_CmpC_Tcs3_SetsFlagsButNotT)
{
    setReg(8, 5u);
    setReg(9, 5u);
    execute(0x8068a419u);                       // cmp.c r8, r9 (TCS 3)
    EXPECT_EQ(getFlag(spg290::Z), 1);
    EXPECT_EQ(getFlag(spg290::C), 1);
    EXPECT_EQ(getFlag(spg290::T), 0);
}

TEST_F(CPUFixture, PreIndexLoad_IntoBaseRegisterKeepsLoadedValue)
{
    setReg(3, 0x100u);
    setMem(0x104, 0x55AA55AAu);
    execute(encodeLW_pre(3, 3, 4));
    EXPECT_EQ(getReg(3), 0x55AA55AAu);
}

TEST_F(CPUFixture, PostIndexLoad_IntoBaseRegisterKeepsUpdatedAddress)
{
    setReg(3, 0x100u);
    setMem(0x100, 0x55AA55AAu);
    execute(encodeLW_post(3, 3, 4));
    EXPECT_EQ(getReg(3), 0x104u);
}

TEST_F(CPUFixture, Cmpis_ComparesAgainstImmediateShiftedBySixteen)
{
    setReg(5, 0x00050000u);
    execute(encodeCMPIS(5, 5));
    EXPECT_EQ(getFlag(spg290::Z), 1);
    EXPECT_EQ(getFlag(spg290::T), 1);

    bus.cpu.pc = 0;
    execute(encodeCMPIS(5, 6));
    EXPECT_EQ(getFlag(spg290::Z), 0);
    EXPECT_EQ(getFlag(spg290::N), 1);
}

TEST_F(CPUFixture, CmpTcs2_SetsTUnconditionally)
{
    setReg(1, 1u);
    setReg(2, 2u);
    execute(encodeCMP(1, 2, 2));
    EXPECT_EQ(getFlag(spg290::T), 1);
}

TEST_F(CPUFixture, BrCond_SpecialForm_BranchesToRegisterWhenConditionHolds)
{
    setReg(5, 0x300u);
    bus.cpu.status |= spg290::Z;
    execute(encodeBR(4 /*eq*/, 5));
    EXPECT_EQ(bus.cpu.pc, 0x300u);
}

TEST_F(CPUFixture, BrlCond_LinksNextInstructionEvenWhenNotTaken)
{
    bus.cpu.pc = 0x40;
    setReg(5, 0x300u);
    bus.cpu.status &= ~spg290::Z;
    execute(encodeBRL(4 /*eq*/, 5));
    EXPECT_EQ(bus.cpu.pc, 0x44u);
    EXPECT_EQ(getReg(3), 0x44u);
}

TEST_F(CPUFixture, MvCond_Ec14_IsNoOp)
{
    setReg(1, 7u);
    setReg(2, 9u);
    execute(encodeMVCOND(1, 2, 14));
    EXPECT_EQ(getReg(1), 7u);
}

TEST_F(CPUFixture, BitOps32_ClearSetToggle)
{
    setReg(5, 0xFFu);
    execute(encodeBITCLR(5, 3, false));
    EXPECT_EQ(getReg(5), 0xF7u);
    execute(encodeBITSET(5, 8, false));
    EXPECT_EQ(getReg(5), 0x1F7u);
    execute(encodeBITTGL(5, 0, false));
    EXPECT_EQ(getReg(5), 0x1F6u);
}

TEST_F(CPUFixture, Cache_Pflush_Sdbbp32_AreNoOps)
{
    execute(encodeCACHE());
    execute(encodePFLUSH());
    execute(encodeSDBBP32());
    EXPECT_EQ(bus.cpu.pc, 12u);
    EXPECT_EQ(bus.cpu.cr[2] & spg290::ECR_CODE_MASK, 0u);
}

TEST_F(CPUFixture, UndefinedSpecialFunc6_RaisesReservedInstruction)
{
    execute(0x80008000u | (0x06u << 1));
    EXPECT_EQ(bus.cpu.cr[2] & spg290::ECR_CODE_MASK, 9u);
    EXPECT_EQ(bus.cpu.cr[5], 0u);
}

TEST_F(CPUFixture, CustomEngineOpcode_RaisesReservedInstruction)
{
    execute(0x80008000u | (0x1Cu << 26));
    EXPECT_EQ(bus.cpu.cr[2] & spg290::ECR_CODE_MASK, 9u);
}

TEST_F(CPUFixture, CoprocessorOpcode_RaisesReservedInstruction)
{
    execute(0x80008000u | (0x1Fu << 26));
    EXPECT_EQ(bus.cpu.cr[2] & spg290::ECR_CODE_MASK, 9u);
}
