/* test_irq.s — Tier-2 GAS test ROM: PPU vblank IRQ and syscall exception.
 *
 * Assembled with score-elf-as -SCORE7, linked at address 0 (score.ld).
 * lw_e/sw_e come from emul_score7.inc (the emulator's load/store imm format);
 * everything else is a native mnemonic. Only r16+ are used with small
 * immediates so GAS never picks a 16-bit "!" form; r30/r31 are handler scratch.
 *
 * Flow:
 *   EXCPvec = 0x1001 (base 0x1000, 16-byte spacing)
 *   PPU BLK_ST_EN on, interrupt controller source 10 unmasked, PSR.IEc on.
 *   The test harness raises a vblank periodically; the IRQ-53 handler at
 *   base + 0x200 + 0x10*54 = 0x1560 counts it at 0x2000 and acknowledges the
 *   PPU status. Main spins until the count reaches 3, then executes syscall.
 *   The general handler at 0x1200 stores ECR to 0x2004, skips the syscall, rte.
 *   Main then reports through the ROM protocol; the C++ test checks the saved
 *   ECR. The real GAS J-form and forward B-form displacements are not decoded
 *   correctly by the emulator yet, so only a short backward bne is native here
 *   and the final halt uses j_e.
 */

.include "emul_score7.inc"

    .text
    .align 2

start:
    li      r16, 0x1001
    mtcr    r16, cr3
    li      r17, 0x88010080         /* P_IRQ_Control */
    ldi     r18, 1                  /* BLK_ST_EN */
    sw_e    18, 17, 0
    li      r17, 0x880A0020         /* P_I_MASKL */
    li      r18, 0xFFFFFBFF         /* unmask source 10 (PPU vblank start) */
    sw_e    18, 17, 0
    li      r19, 0x2000
    ldi     r20, 0
    sw_e    20, 19, 0               /* irq counter = 0 */
    sw_e    20, 19, 4               /* saved ECR   = 0 */
    mfcr    r16, cr0
    ori     r16, 1                  /* PSR.IEc */
    mtcr    r16, cr0
    ldi     r21, 3
wait:
    lw_e    22, 19, 0
    sub.c   r23, r22, r21
    bne     wait
    syscall 0x8E
    li      r27, 0xDEAD             /* the C++ test checks the saved ECR */
    li      r28, 0xFF00
    sw_e    27, 28, 4               /* ROM_RESULT_ADDR */
    ldi     r29, 1
    sw_e    29, 28, 0               /* ROM_DONE_ADDR */

    .org 0x100
halt:
    j_e     0x80                    /* jump to self (byte 0x100) */

    .org 0x1200
general_handler:
    mfcr    r31, cr2
    li      r30, 0x2000
    sw_e    31, 30, 4
    mfcr    r31, cr5
    addi    r31, 4                  /* skip the 32-bit syscall */
    mtcr    r31, cr5
    rte

    .org 0x1560
irq53_handler:
    li      r30, 0x2000
    lw_e    31, 30, 0
    addi    r31, 1
    sw_e    31, 30, 0
    li      r30, 0x88010084         /* P_IRQ_Status */
    ldi     r31, 1                  /* write 1 to clear BLK_IRQ_ST */
    sw_e    31, 30, 0
    rte
