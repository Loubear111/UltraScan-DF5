#include "test_helpers.h"

// ============================================================
// Conditional test (t{cond}), conditional/unconditional traps
// (trap{cond}, syscall), exception returns (rte/drte), the
// no-op system controls (sleep/cache/pflush/sdbbp/ceinst), and
// coprocessor load/store (ldcx/stcx). All live under OP_SYS
// (opcode 0x1F).
//
// Exception model (minimal): the faulting instruction address is
// saved into EPC (cr5), the cause into ECR (cr2), and control
// vectors to EXCPvec (cr3). rte returns to EPC, drte to DEPC (cr30).
// ============================================================

// ---- t{cond} ----
TEST_F(CPUFixture, TCOND_EQ_SetsTWhenZero)
{
    bus.cpu.status |= spg290::Z;
    execute(encodeTCOND(4));          // EQ
    EXPECT_EQ(getFlag(spg290::T), 1u);
}

TEST_F(CPUFixture, TCOND_EQ_ClearsTWhenNotZero)
{
    bus.cpu.status |= spg290::T;       // start set to prove it gets cleared
    execute(encodeTCOND(4));          // EQ, Z is clear
    EXPECT_EQ(getFlag(spg290::T), 0u);
}

TEST_F(CPUFixture, TCOND_CS_TracksCarry)
{
    bus.cpu.status |= spg290::C;
    execute(encodeTCOND(0));          // CS
    EXPECT_EQ(getFlag(spg290::T), 1u);
}

TEST_F(CPUFixture, TCOND_Always_SetsT)
{
    execute(encodeTCOND(15));         // AL
    EXPECT_EQ(getFlag(spg290::T), 1u);
}

// ---- trap{cond} ----
TEST_F(CPUFixture, TRAPCOND_Always_VectorsToHandler)
{
    bus.cpu.cr[3] = 0x40;             // EXCPvec
    execute(encodeTRAPCOND(15));      // AL -> always trap
    EXPECT_EQ(bus.cpu.pc, 0x40u);     // jumped to handler
    EXPECT_EQ(bus.cpu.cr[2], 10u);    // ECR exc_code = trap
    EXPECT_EQ(bus.cpu.cr[5], 0u);     // EPC = faulting instr (was at pc 0)
}

TEST_F(CPUFixture, TRAPCOND_NoOpCode_DoesNotTrap)
{
    bus.cpu.cr[3] = 0x40;
    execute(encodeTRAPCOND(14));      // EC=14 is "no operation"
    EXPECT_EQ(bus.cpu.pc, 1u);        // simply advanced past the instruction
    EXPECT_EQ(bus.cpu.cr[2], 0u);     // ECR untouched
}

TEST_F(CPUFixture, TRAPCOND_EQ_TrapsWhenZeroSet)
{
    bus.cpu.cr[3] = 0x50;
    bus.cpu.status |= spg290::Z;
    execute(encodeTRAPCOND(4));       // EQ
    EXPECT_EQ(bus.cpu.pc, 0x50u);
    EXPECT_EQ(bus.cpu.cr[2], 10u);
}

TEST_F(CPUFixture, TRAPCOND_EQ_NoTrapWhenZeroClear)
{
    bus.cpu.cr[3] = 0x50;
    execute(encodeTRAPCOND(4));       // EQ, Z clear -> no trap
    EXPECT_EQ(bus.cpu.pc, 1u);
}

// ---- syscall ----
TEST_F(CPUFixture, SYSCALL_AlwaysTraps)
{
    bus.cpu.cr[3] = 0x60;
    execute(encodeSYSCALL());
    EXPECT_EQ(bus.cpu.pc, 0x60u);     // vectored to handler
    EXPECT_EQ(bus.cpu.cr[2], 7u);     // ECR exc_code = syscall
    EXPECT_EQ(bus.cpu.cr[5], 0u);     // EPC = syscall instruction
}

// ---- rte / drte ----
TEST_F(CPUFixture, RTE_ReturnsToEPC)
{
    bus.cpu.cr[5] = 0x55;             // EPC
    execute(encodeRTE());
    EXPECT_EQ(bus.cpu.pc, 0x55u);
}

TEST_F(CPUFixture, DRTE_ReturnsToDEPC)
{
    bus.cpu.cr[30] = 0x77;            // DEPC
    execute(encodeDRTE());
    EXPECT_EQ(bus.cpu.pc, 0x77u);
}

// ---- no-op system controls ----
TEST_F(CPUFixture, SystemNoOps_AdvanceWithoutSideEffects)
{
    bus.cpu.status |= spg290::C;       // flags must survive untouched
    execute(encodeSLEEP());
    EXPECT_EQ(bus.cpu.pc, 1u);
    execute(encodeCACHE());
    execute(encodePFLUSH());
    execute(encodeSDBBP32());
    execute(encodeCEINST());
    EXPECT_EQ(getFlag(spg290::C), 1u);
}

// ---- ldcx / stcx ----
TEST_F(CPUFixture, LDCX_LoadsCoprocessorDataFromMemory)
{
    setReg(5, 100);                   // base r5 = 100
    setMem(104,  0xDEADBEEFu);         // memory at 100 + 4
    execute(encodeLDCX(5, 8, 2, 4));  // ldc2 cr8, [r5, 4]
    EXPECT_EQ(bus.cpu.copData[2][8], 0xDEADBEEFu);
}

TEST_F(CPUFixture, LDCX_NegativeOffset)
{
    setReg(5, 100);
    setMem(96,  0x12345678u);          // memory at 100 - 4
    execute(encodeLDCX(5, 3, 1, -4));
    EXPECT_EQ(bus.cpu.copData[1][3], 0x12345678u);
}

TEST_F(CPUFixture, STCX_StoresCoprocessorDataToMemory)
{
    setReg(5, 100);
    bus.cpu.copData[3][9] = 0xCAFEBABEu;
    execute(encodeSTCX(5, 9, 3, 8));  // stc3 cr9, [r5, 8]
    EXPECT_EQ(getMem(108), 0xCAFEBABEu);
}

TEST_F(CPUFixture, STCX_then_LDCX_RoundTrip)
{
    setReg(6, 50);
    bus.cpu.copData[2][4] = 0x0BADF00Du;
    execute(encodeSTCX(6, 4, 2, 2));  // mem[52] = cr4
    bus.cpu.copData[2][4] = 0;        // clear to prove the reload works
    execute(encodeLDCX(6, 4, 2, 2));  // cr4 = mem[52]
    EXPECT_EQ(bus.cpu.copData[2][4], 0x0BADF00Du);
}
