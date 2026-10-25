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

/* sw_e: OP_SW=0x14 (confirmed), Imm15 at bits 14:0, bit 15 = P-bit. */
.macro sw_e rd, ra, wo
    .long (0x80000000 | (0x14 << 26) | ((\rd) << 21) | ((\ra) << 16) | 0x8000 | ((\wo) & 0x7FFF))
.endm

/* lw_e: OP_LW=0x10, same format as sw_e. */
.macro lw_e rd, ra, wo
    .long (0x80000000 | (0x10 << 26) | ((\rd) << 21) | ((\ra) << 16) | 0x8000 | ((\wo) & 0x7FFF))
.endm

/* j_e: OP_JX=0x02 (confirmed), 24-bit absolute word address.
 * Disp24[23:14] at bits 25:16, Disp24[13:0] at bits 14:1, bit 15 = P-bit. */
.macro j_e wa
    .long (0x80000000 | (0x02 << 26) | (((((\wa) >> 14) & 0x3FF)) << 16) | 0x8000 | (((\wa) & 0x3FFF) << 1))
.endm

/* beq_e: 32-bit BCOND opcode=0x04 (confirmed via score-elf-as), bc=4 (EQ),
 * split 19-bit disp: Disp[18:9] at bits25:16, bit 15 = P-bit, Disp[8:0] at bits9:1. */
.macro beq_e disp
    .long (0x80000000 | (0x04 << 26) | ((((\disp) >> 9) & 0x3FF) << 16) | 0x8000 | (4 << 10) | (((\disp) & 0x1FF) << 1))
.endm
