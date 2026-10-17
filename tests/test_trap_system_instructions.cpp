#include "test_helpers.h"

// ============================================================
// Conditional test (t{cond}), conditional/unconditional traps
// (trap{cond}, syscall), exception returns (rte/drte), the
// no-op system controls (cache/pflush/sdbbp/ceinst), and
// coprocessor load/store (ldcx/stcx). rte/drte/sleep are CR-form
// (opcode 0x06); the rest live under OP_SYS (opcode 0x1F) or R-type.
//
// Exception model: PSR.{UMs,IEs} <- {UMc,IEc} and the current pair is
// cleared, the faulting instruction address goes to EPC (cr5), the
// cause to ECR (cr2), and control vectors to EXCPvec (cr3) base + 0x200.
// rte undoes the PSR shift and returns to EPC, drte to DEPC (cr30).
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
    bus.cpu.cr[3] = 0x40;             // EXCPvec, 4-byte spacing
    execute(encodeTRAPCOND(15));      // AL -> always trap
    EXPECT_EQ(bus.cpu.pc, 0x240u);    // general vector = base + 0x200
    EXPECT_EQ(bus.cpu.cr[2], 10u);    // ECR exc_code = trap
    EXPECT_EQ(bus.cpu.cr[5], 0u);     // EPC = faulting instr (was at pc 0)
}

TEST_F(CPUFixture, TRAPCOND_NoOpCode_DoesNotTrap)
{
    bus.cpu.cr[3] = 0x40;
    execute(encodeTRAPCOND(14));      // EC=14 is "no operation"
    EXPECT_EQ(bus.cpu.pc, 4u);        // simply advanced past the instruction
    EXPECT_EQ(bus.cpu.cr[2], 0u);     // ECR untouched
}

TEST_F(CPUFixture, TRAPCOND_EQ_TrapsWhenZeroSet)
{
    bus.cpu.cr[3] = 0x50;
    bus.cpu.status |= spg290::Z;
    execute(encodeTRAPCOND(4));       // EQ
    EXPECT_EQ(bus.cpu.pc, 0x250u);
    EXPECT_EQ(bus.cpu.cr[2], 10u);
}

TEST_F(CPUFixture, TRAPCOND_EQ_NoTrapWhenZeroClear)
{
    bus.cpu.cr[3] = 0x50;
    execute(encodeTRAPCOND(4));       // EQ, Z clear -> no trap
    EXPECT_EQ(bus.cpu.pc, 4u);
}

// ---- syscall ----
TEST_F(CPUFixture, SYSCALL_AlwaysTraps)
{
    bus.cpu.cr[3] = 0x61;             // 16-byte spacing: mode bit set
    execute(encodeSYSCALL());
    EXPECT_EQ(bus.cpu.pc, 0x260u);    // vectored to base(0x60) + 0x200
    EXPECT_EQ(bus.cpu.cr[2], 7u);     // ECR exc_code = syscall
    EXPECT_EQ(bus.cpu.cr[5], 0u);     // EPC = syscall instruction
}

TEST_F(CPUFixture, Exception_ShiftsPSRAndClearsIE)
{
    bus.cpu.cr[0] = spg290::PSR_IEC;
    execute(encodeSYSCALL());
    EXPECT_EQ(bus.cpu.cr[0] & spg290::PSR_IEC, 0u);
    EXPECT_NE(bus.cpu.cr[0] & spg290::PSR_IES, 0u);
}

TEST_F(CPUFixture, Exception_SecondEntryOverwritesSavedIE)
{
    bus.cpu.cr[0] = spg290::PSR_IEC;
    execute(encodeSYSCALL());          // IEs = 1, IEc = 0
    bus.write32(bus.cpu.pc, encodeSYSCALL());
    bus.cpu.clock();                   // IEs <- IEc (0): single-level shadow
    EXPECT_EQ(bus.cpu.cr[0] & spg290::PSR_IES, 0u);
}

TEST_F(CPUFixture, Exception_RteRestoresPSRAndFlags)
{
    bus.cpu.cr[0] = spg290::PSR_IEC;
    bus.cpu.status |= spg290::C | spg290::Z;
    execute(encodeSYSCALL());
    bus.cpu.status = 0;                // handler clobbers the flags
    execute(encodeRTE());
    EXPECT_NE(bus.cpu.cr[0] & spg290::PSR_IEC, 0u);
    EXPECT_EQ(getFlag(spg290::C), 1u);
    EXPECT_EQ(getFlag(spg290::Z), 1u);
    EXPECT_EQ(bus.cpu.pc, 0u);         // EPC = the syscall itself
}

TEST_F(CPUFixture, CR1_MapsToConditionFlags)
{
    bus.cpu.status = spg290::C | spg290::T;
    execute(encodeMFCR(4, 1));
    EXPECT_EQ(getReg(4), (uint32_t)(spg290::C | spg290::T));
    setReg(5, spg290::Z);
    execute(encodeMTCR(5, 1));
    EXPECT_EQ(bus.cpu.status, (uint32_t)spg290::Z);
}

// ---- rte / drte ----
TEST_F(CPUFixture, RTE_ReturnsToEPC)
{
    bus.cpu.cr[5] = 0x55;             // EPC (bit 0 is masked off)
    execute(encodeRTE());
    EXPECT_EQ(bus.cpu.pc, 0x54u);
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
