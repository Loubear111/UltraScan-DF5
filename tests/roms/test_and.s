/* test_and.s — Tier-2 GAS test ROM: exercise the ANDX instruction.
 *
 * Assembled with: score-elf-as -SCORE7  (binutils 2.25, big-endian Score7)
 * Linked with:    score.ld (places .text at byte address 0)
 *
 * ==========================================================================
 * NATIVE vs MACRO ENCODING — read this before editing
 * ==========================================================================
 * Two kinds of instruction appear in this file:
 *
 *   [real GAS]     — standard Score7 mnemonic, confirmed compatible with the
 *                    emulator's decoder (opcode/func6 match):
 *                      and, and.c, or, or.c, nop
 *
 *   [emulator]     — a macro from emul_score7.inc that emits a .long with the
 *                    emulator's non-standard encoding.  Replace with the
 *                    corresponding native mnemonic once the emulator's
 *                    opcode/func6/func3 constants are corrected:
 *                      addi_e  → addi
 *                      sub_c_e → sub.c
 *                      ori_e   → ori
 *                      sw_e    → sw
 *                      j_e     → j
 *                      beq_e   → beq (32-bit form)
 * ==========================================================================
 *
 * Test sequence (one test case):
 *   r1 = 0x0F0F, r2 = 0x00FF
 *   r3 = r1 & r2   (expected: 0x000F)
 *   if r3 == 0x000F → write PASS_VALUE (0xDEAD) to ROM_RESULT_ADDR
 *   else            → write error code 1 to ROM_RESULT_ADDR
 *   write 1 to ROM_DONE_ADDR and loop forever.
 *
 * ROM protocol (matches ROMFixture in test_helpers.h):
 *   ROM_DONE_ADDR   = 0xFF00  (word address)
 *   ROM_RESULT_ADDR = 0xFF01  (word address)
 *   PASS_VALUE      = 0xDEAD
 *
 * Word layout:
 *   [0]  addi_e r1, 0x0F0F       r1 = 0x0F0F
 *   [1]  addi_e r2, 0x00FF       r2 = 0x00FF
 *   [2]  and.c  r3, r1, r2       r3 = r1 & r2 = 0x000F; sets flags
 *   [3]  addi_e r4, 0x000F       r4 = expected result
 *   [4]  sub_c_e r5, r3, r4      r5 = r3 - r4; Z=1 if equal
 *   [5]  beq_e 4                 if Z: jump to [9]  (pc_after_fetch=6; 6-1+4=9)
 *   [6]  addi_e r6, 1            FAIL: r6 = error code 1
 *   [7]  j_e 10                  jump to WRITE_RESULT [10]
 *   [8]  nop                     padding
 *   [9]  ori_e r6, 0xDEAD        PASS: r6 = PASS_VALUE
 *  [10]  ori_e r8, 0xFF00        r8 = ROM_DONE_ADDR base
 *  [11]  sw_e  r6, r8, 1         Mem[r8+1] = r6 → ROM_RESULT_ADDR (0xFF01)
 *  [12]  addi_e r7, 1            r7 = done flag (1)
 *  [13]  sw_e  r7, r8, 0         Mem[r8+0] = r7 → ROM_DONE_ADDR (0xFF00)
 *  [14]  j_e 14                  HALT: infinite loop (jumps to self)
 */

.include "emul_score7.inc"

    .text
    .align 2

    /* ── Load test operands ─────────────────────────────────────── */
    addi_e   1, 0x0F0F          /* [emulator] r1 = 0x0F0F              [0] */
    addi_e   2, 0x00FF          /* [emulator] r2 = 0x00FF              [1] */

    /* ── The instruction under test ─────────────────────────────── */
    and.c    r3, r1, r2         /* [real GAS] r3 = r1 & r2 = 0x000F   [2] */

    /* ── Compare result against expected ────────────────────────── */
    addi_e   4, 0x000F          /* [emulator] r4 = expected = 0x000F   [3] */
    sub_c_e  5, 3, 4            /* [emulator] r5 = r3 - r4; Z=1 if ==  [4] */
    beq_e    4                  /* [emulator] Z set → jump to [9]      [5] */

    /* ── FAIL path ───────────────────────────────────────────────── */
    addi_e   6, 1               /* [emulator] r6 = error code 1        [6] */
    j_e      10                 /* [emulator] jump to WRITE_RESULT     [7] */
    nop                         /* [real GAS] padding                  [8] */

    /* ── PASS path [9] ───────────────────────────────────────────── */
    ori_e    6, 0xDEAD          /* [emulator] r6 = PASS_VALUE (0xDEAD) [9] */

    /* ── Write result and signal done [10] ──────────────────────── */
    ori_e    8, 0xFF00          /* [emulator] r8 = 0xFF00             [10] */
    sw_e     6, 8, 1            /* [emulator] Mem[r8+1]=r6 →RESULT    [11] */
    addi_e   7, 1               /* [emulator] r7 = done flag = 1      [12] */
    sw_e     7, 8, 0            /* [emulator] Mem[r8+0]=r7 → DONE     [13] */

    /* ── HALT [14] ───────────────────────────────────────────────── */
    j_e      14                 /* [emulator] infinite loop           [14] */
