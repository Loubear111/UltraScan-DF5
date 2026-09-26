/* emul_score7.inc -- Score7 test ROM compatibility macros (post 2026-06-20 fix).
 *
 * Confirmed native mnemonics (addi/sub.c/ori have matching encodings now):
 *   addi_e -> addi rD, imm,  sub_c_e -> sub.c rD,rA,rB,  ori_e -> ori rD, imm
 *
 * Still .long (LS imm format or label-vs-number difference):
 *   sw_e (OP_SW=0x14), j_e (OP_JX=0x02), beq_e (OP_BCOND=0x18)
 */

.macro addi_e rd, imm
    addi    r\rd, \imm
.endm

.macro sub_c_e rd, ra, rb
    sub.c   r\rd, r\ra, r\rb
.endm

.macro ori_e rd, imm
    ori     r\rd, \imm
.endm

/* sw_e: OP_SW=0x14 (confirmed), emulator imm format bits 15:1. */
.macro sw_e rd, ra, wo
    .long (0x80000000 | (0x14 << 26) | ((\rd) << 21) | ((\ra) << 16) | (((\wo) & 0x7FFF) << 1))
.endm

/* j_e: OP_JX=0x02 (confirmed), 24-bit absolute word address. */
.macro j_e wa
    .long (0x80000000 | (0x02 << 26) | (((\wa) & 0xFFFFFF) << 1))
.endm

/* beq_e: 32-bit BCOND opcode=0x18, bc=4 (EQ), signed disp. */
.macro beq_e disp
    .long (0x80000000 | (0x18 << 26) | (4 << 22) | (((\disp) & 0x1FFFFF) << 1))
.endm
