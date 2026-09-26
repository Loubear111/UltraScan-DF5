#include "test_helpers.h"

// ============================================================
// Tier-1 smoke test ROM — hand-encoded uint32_t program.
//
// Demonstrates the ROMFixture end-to-end:
//   - Instructions are loaded at address 0 (PC starts at 0).
//   - CPU registers (r0-r31) are separate from bus RAM.
//   - ROM writes PASS_VALUE to ROM_RESULT_ADDR when all checks pass.
//   - ROM writes a non-zero error code on failure.
//   - ROM writes 1 to ROM_DONE_ADDR to signal completion.
//
// Instructions used (all dispatched in clock()):
//   ADDIX r, imm16   : r += sext(imm16)  (r0 always 0 → load immediate)
//   ORIX  r, imm16   : r |= zero_ext(imm16)  (loads values > 0x7FFF)
//   ANDX  rD, rA, rB : rD = rA & rB, updates flags
//   SUBX  rD, rA, rB : rD = rA - rB, updates flags (sets Z if equal)
//   BCOND bc, disp   : branch if condition holds (bc=4 → EQ/Z set)
//   JX    disp       : unconditional absolute jump
//   SW    rD, rA, imm: Mem[rA + sext(imm)] = rD
//
// Address strategy: ROM_DONE_ADDR (0xFF00) and ROM_RESULT_ADDR (0xFF01)
// are too large for SW's 15-bit signed immediate.  Instead we load the
// base 0xFF00 into r8 via ORIX, then use imm=0 and imm=1 offsets:
//   Mem[r8+1] = result     (→ 0xFF01 = ROM_RESULT_ADDR)
//   Mem[r8+0] = done flag  (→ 0xFF00 = ROM_DONE_ADDR)
// ============================================================

// -----------------------------------------------------------------
// ROM program:                               (PC word offset)
//
//   ADDIX r1, 0x0F0F          ; r1 = 0x0F0F              [0]
//   ADDIX r2, 0x00FF          ; r2 = 0x00FF              [1]
//   ANDX  r3, r1, r2, CU=1   ; r3 = 0x000F, sets Z/N    [2]
//   ADDIX r4, 0x000F          ; r4 = expected 0x000F     [3]
//   SUBX  r5, r3, r4, CU=1   ; r5 = r3-r4, sets Z if 0  [4]
//   BCOND EQ(4), disp=4       ; if Z: jump to [9]        [5]
//   ADDIX r6, 1               ; FAIL: error code 1       [6]
//   JX    10                  ; jump to WRITE_RESULT      [7]
//   NOP                       ; (padding)                [8]
//   ORIX  r6, 0xDEAD          ; PASS: r6 = PASS_VALUE    [9]
//   ORIX  r8, 0xFF00          ; WRITE_RESULT: r8 = 0xFF00 [10]
//   SW    r6, r8, 1           ; Mem[0xFF01] = result     [11]
//   ADDIX r7, 1               ; r7 = done flag           [12]
//   SW    r7, r8, 0           ; Mem[0xFF00] = done       [13]
//   JX    14                  ; halt (branch to self)    [14]
//
// BCOND at [5]: pc after fetch = 6.  Formula: pc = (pc-1) + disp.
//   Target = 9 → disp = 9 - (6-1) = 4.  ✓
// -----------------------------------------------------------------

static const uint32_t andx_smoke_rom[] = {
    /* 0  */ encodeADDIX(1, 0x0F0F, false),        // r1 = 0x0F0F
    /* 1  */ encodeADDIX(2, 0x00FF, false),        // r2 = 0x00FF
    /* 2  */ encodeANDX (3, 1, 2, true),           // r3 = r1 & r2 = 0x000F; sets flags
    /* 3  */ encodeADDIX(4, 0x000F, false),        // r4 = 0x000F (expected)
    /* 4  */ encodeSUBX (5, 3, 4, true),           // r5 = r3 - r4; Z=1 if equal
    /* 5  */ encodeBCOND(4, 4),                    // EQ: if Z jump to [9]
    /* 6  */ encodeADDIX(6, 1, false),             // FAIL: r6 = error code 1
    /* 7  */ encodeJX   (10, false),               // jump to WRITE_RESULT [10]
    /* 8  */ encodeNOP  (),                        // padding
    /* 9  */ encodeORIX (6, 0xDEADu, false),       // PASS: r6 = 0xDEAD
    /* 10 */ encodeORIX (8, 0xFF00u, false),       // r8 = ROM_DONE_ADDR (0xFF00)
    /* 11 */ encodeSW   (6, 8, 1),                 // Mem[r8+1] = r6  → ROM_RESULT_ADDR
    /* 12 */ encodeADDIX(7, 1, false),             // r7 = 1 (done flag)
    /* 13 */ encodeSW   (7, 8, 0),                 // Mem[r8+0] = r7  → ROM_DONE_ADDR
    /* 14 */ encodeJX   (14, false),               // halt: branch to self
};

TEST_F(ROMFixture, ANDX_SmokeROM_Passes)
{
    runROM(andx_smoke_rom, std::size(andx_smoke_rom));
    EXPECT_TRUE(romPassed()) << "ROM result: 0x" << std::hex << romResult();
}

// Mutation test: wrong expected value forces the FAIL path.
// Confirms the ROM harness actually catches incorrect results.
TEST_F(ROMFixture, ANDX_SmokeROM_FailsWithWrongExpected)
{
    static const uint32_t mutated_rom[] = {
        /* 0  */ encodeADDIX(1, 0x0F0F, false),
        /* 1  */ encodeADDIX(2, 0x00FF, false),
        /* 2  */ encodeANDX (3, 1, 2, true),
        /* 3  */ encodeADDIX(4, 0x0001, false),    // WRONG expected → fail path
        /* 4  */ encodeSUBX (5, 3, 4, true),
        /* 5  */ encodeBCOND(4, 4),
        /* 6  */ encodeADDIX(6, 1, false),
        /* 7  */ encodeJX   (10, false),
        /* 8  */ encodeNOP  (),
        /* 9  */ encodeORIX (6, 0xDEADu, false),
        /* 10 */ encodeORIX (8, 0xFF00u, false),
        /* 11 */ encodeSW   (6, 8, 1),
        /* 12 */ encodeADDIX(7, 1, false),
        /* 13 */ encodeSW   (7, 8, 0),
        /* 14 */ encodeJX   (14, false),
    };
    runROM(mutated_rom, std::size(mutated_rom));
    EXPECT_FALSE(romPassed());
    EXPECT_EQ(romResult(), 1u);  // error code 1 from FAIL path
}

// ============================================================
// Tier-1 smoke test ROM — hand-encoded uint32_t program.
//
// Demonstrates the ROMFixture end-to-end:
//   - Instructions are loaded at address 0 (PC starts at 0).
//   - CPU registers (r0-r31) are separate from bus RAM.
//   - ROM writes PASS_VALUE to ROM_RESULT_ADDR when all checks pass.
//   - ROM writes a non-zero error code on failure.
//   - ROM writes 1 to ROM_DONE_ADDR to signal completion.
//
// Instructions used (all dispatched in clock()):
//   ORIX  r, 0xDEAD  : r = r0 | 0xDEAD  (zero-extended → 0x0000DEAD)
//   ADDIX r, imm16   : r = r + sext(imm) (r0 always 0 → load immediate)
//   ANDX  rD, rA, rB : rD = rA & rB
//   SUBX  rD, rA, rB : rD = rA - rB, sets Z flag if equal
//   BCOND bc, disp   : branch if condition holds (bc=4 → EQ/Z set)
//   SW    rD, rA, imm: Mem[rA + imm] = rD  (rA=r0=0 → store to address imm)
//   JX    disp, lk   : unconditional jump to absolute address
// ============================================================

// ROM memory layout (word offsets):
//   [0 ..N-1]  : instructions
//   [0xFF00]   : ROM_DONE_ADDR
//   [0xFF01]   : ROM_RESULT_ADDR

// Helper: r0 is hardwired to 0, so  ADDIX rD, imm  loads sext(imm) into rD.
//         ORIX  rD, imm  loads zero_ext(imm) into rD (from r0=0 base).

// -----------------------------------------------------------------
// ROM program:
//
//   r1 = 0x0F0F                     (ADDIX  r1, 0x0F0F)
//   r2 = 0x00FF                     (ADDIX  r2, 0x00FF)
//   r3 = r1 & r2, set flags         (ANDX   r3, r1, r2, CU=1)
//   r4 = 0x000F  (expected)         (ADDIX  r4, 0x000F)
//   r5 = r3 - r4, set flags         (SUBX   r5, r3, r4, CU=1)
//   if (Z) goto PASS                (BCOND  EQ=4, disp=+3)
//   ;; FAIL path ;;
//   r6 = 1  (error code: case 1)    (ADDIX  r6, 1)
//   Mem[ROM_RESULT_ADDR] = r6       (SW     r6, r0, ROM_RESULT_ADDR)
//   goto DONE                       (JX     DONE_INSTR, lk=0)
//   ;; PASS path ;;
//   r6 = 0xDEAD  (PASS_VALUE)       (ORIX   r6, 0xDEAD)
//   Mem[ROM_RESULT_ADDR] = r6       (SW     r6, r0, ROM_RESULT_ADDR)
//   ;; DONE ;;
//   r7 = 1  (done flag)             (ADDIX  r7, 1)
//   Mem[ROM_DONE_ADDR] = r7         (SW     r7, r0, ROM_DONE_ADDR)
//   halt: JX halt                   (JX     HALT_INSTR, lk=0)
// -----------------------------------------------------------------

//  PC offsets (word-addressed, each instruction is 1 word):
//   0  ADDIX r1, 0x0F0F
//   1  ADDIX r2, 0x00FF
//   2  ANDX  r3, r1, r2
//   3  ADDIX r4, 0x000F
//   4  SUBX  r5, r3, r4
//   5  BCOND EQ, +4          (if Z: jump to PC=5+4=9, i.e. PASS path)
//   6  ADDIX r6, 1           -- FAIL path --
//   7  SW    r6, r0, ROM_RESULT_ADDR
//   8  JX    12              (jump to DONE at PC=12)
//   9  ORIX  r6, 0xDEAD      -- PASS path --
//  10  SW    r6, r0, ROM_RESULT_ADDR
//  11  NOP (pad)
//  12  ADDIX r7, 1            -- DONE --
//  13  SW    r7, r0, ROM_DONE_ADDR
//  14  JX    14               (halt: branch to self)

// BCOND displacement: BCOND branches to (PC_after_fetch - 1) + disp.
// After fetching word 5, PC = 6.  We want to land at word 9.
// disp = 9 - (6 - 1) = 9 - 5 = 4.
// (The implementation: pc = (pc - 1) + disp, where pc has already been
//  incremented by clock(). So after the fetch of instr[5], pc=6.
//  We want pc = 9 → disp = 9 - (6 - 1) = 4.)

