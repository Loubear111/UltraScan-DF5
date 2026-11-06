// Cross-checks the spec-derived instruction encoders in test_helpers.h against an
// independent decoder: MAME's S+core7 disassembler (tools/sdis, BSD-3-Clause).
//
// Nothing here executes on the emulator. A failure means an encoder (or the
// reference disassembler's reading of the same bits) disagrees with the
// architecture layout, which is exactly the self-confirming-test loop this
// file exists to break. See docs/phase4a-conformance-fix-plan.md, step B.
#include <gtest/gtest.h>
#include <cstdint>
#include <string>
#include <vector>

#include "score_disasm.h"
#include "test_helpers.h"

namespace {

struct Case32 {
    const char* what;
    uint32_t    word;
    const char* text;
    uint32_t    pc;        // address the instruction is disassembled at
    bool        prefix;    // compare only the leading text (MAME prints some operands sloppily)
};

struct Case16 {
    const char* what;
    uint16_t    word;
    const char* text;
    uint32_t    pc;
    bool        prefix;
};

bool textMatches(const std::string& got, const char* want, bool prefix)
{
    return prefix ? got.compare(0, std::string(want).size(), want) == 0 : got == want;
}

void check32(const Case32& c)
{
    SCOPED_TRACE(c.what);
    EXPECT_EQ(c.word & 0x80008000u, 0x80008000u) << "both P-bits must be set on a 32-bit instruction";
    int size = 0;
    const std::string got = score7Disasm(c.pc, c.word, size);
    EXPECT_EQ(size, 4) << got;
    EXPECT_TRUE(textMatches(got, c.text, c.prefix)) << "got \"" << got << "\", want \"" << c.text << "\"";
}

void check16(const Case16& c)
{
    SCOPED_TRACE(c.what);
    EXPECT_EQ(c.word & 0x8000u, 0u) << "P-bit (bit 15) must be clear on a 16-bit instruction";
    int size = 0;
    const std::string got = score7Disasm(c.pc, c.word, size);
    EXPECT_EQ(size, 2) << got;
    EXPECT_TRUE(textMatches(got, c.text, c.prefix)) << "got \"" << got << "\", want \"" << c.text << "\"";
}

// Condition codes (EC field), in MAME's table order.
constexpr uint8_t EC_EQ = 4, EC_NE = 5, EC_AL = 15;

} // namespace

// ---- P-bit packing ----------------------------------------------------------

TEST(EncodingConformance, PackInsertsBothPBitsAndIsReversible)
{
    const uint32_t ops[] = { 0x00000000u, 0x3FFFFFFFu, 0x2AAAAAAAu, 0x15555555u, 0x06207FE4u, 0x00008000u, 0x00007FFFu };
    for (uint32_t op : ops) {
        const uint32_t raw = enc32_pack(op);
        EXPECT_EQ(raw & 0x80008000u, 0x80008000u);
        // The reference decoder's P-bit removal: op = (raw & 0x7FFF) | ((raw >> 1) & 0x3FFF8000).
        EXPECT_EQ((raw & 0x7FFFu) | ((raw >> 1) & 0x3FFF8000u), op) << std::hex << op;
    }
}

// ---- Real words taken from the HyperScan demos ---------------------------------
// Each word is both disassembled (it must read as the instruction we think it is)
// and reproduced by the matching encoder.

TEST(EncodingConformance, RealDemoWords32)
{
    EXPECT_EQ(encodeSW_pre(2, 0, -4),  0x8c40ffe4u);
    EXPECT_EQ(encodeSB_post(10, 8, 1), 0x9d48800fu);
    EXPECT_EQ(encodeCMP(8, 9, 3),      0x8068a419u);
    EXPECT_EQ(encodeMTCR(4, 0),        0x98808000u);
    EXPECT_EQ(encodeLDIS(28, 0xa009),  0x979ac012u);
    EXPECT_EQ(encodeORIX(28, 0x51e0, false), 0x8795a3c0u);
    EXPECT_EQ(encodeLDI(4, 1),         0x84988002u);
    EXPECT_EQ(encodeNOP(),             0x80008000u);

    const Case32 cases[] = {
        { "sw pre-index",  0x8c40ffe4u, "sw r2, [r0, -4]+",   0, false },
        { "sb post-index", 0x9d48800fu, "sb r10, [r8]+, 1",   0, false },
        { "cmp.c",         0x8068a419u, "cmp.c r8, r9",       0, false },
        { "mtcr",          0x98808000u, "mtcr r4, cr0",       0, false },
        { "ldis",          0x979ac012u, "ldis r28, 0xa009",   0, false },
        { "ori",           0x8795a3c0u, "ori r28, 0x51e0",    0, false },
        { "ldi",           0x84988002u, "ldi r4, 0x0001",     0, false },
        { "nop",           0x80008000u, "nop",                0, false },
    };
    for (const auto& c : cases) check32(c);
}

TEST(EncodingConformance, JumpEncoding)
{
    // j 0x000910d0 at 0x910e0 in the colors demo: word 0x881290d0 (Disp24 = 0x910d0 >> 1).
    EXPECT_EQ(encodeJX(0x910d0u >> 1, false), 0x881290d0u);
    const Case32 c = { "j", 0x881290d0u, "j 0x000910d0", 0x910e0, false };
    check32(c);
}

TEST(EncodingConformance, RealDemoWords16)
{
    EXPECT_EQ(encodePUSH16(2, 0, false), 0x220e);
    EXPECT_EQ(encodeBRCOND16(EC_AL, 3),  0x0f34);
    EXPECT_EQ(encodeLDIU16(6, 3),        0x5603);
    EXPECT_EQ(encodeSW16(6, 7),          0x267c);
    EXPECT_EQ(encodeNOP16(),             0x0000);
    EXPECT_EQ(encodeB16(EC_AL, 0),       0x4F00);

    const Case16 cases[] = {
        { "push!",  0x220e, "push! r2, [r0]",  0, false },
        { "br! r3", 0x0f34, "br! r3",          0, false },
        { "ldiu!",  0x5603, "ldiu! r6, 0x03",  0, false },
        { "sw!",    0x267c, "sw! r6, [r7]",    0, false },
        { "nop!",   0x0000, "nop!",            0, false },
        { "b!",     0x4F00, "b! 0x00000000",   0, false },
    };
    for (const auto& c : cases) check16(c);
}

// ---- 32-bit special form ---------------------------------------------------------

TEST(EncodingConformance, SpecialFormArithmeticAndLogic)
{
    const Case32 cases[] = {
        { "add",     encodeADDX(1, 2, 3, false),   "add r1, r2, r3",     0, false },
        { "add.c",   encodeADDX(1, 2, 3, true),    "add.c r1, r2, r3",   0, false },
        { "addc",    encodeADDCX(1, 2, 3, false),  "addc r1, r2, r3",    0, false },
        { "sub",     encodeSUBX(1, 2, 3, false),   "sub r1, r2, r3",     0, false },
        { "subc",    encodeSUBCX(1, 2, 3, true),   "subc.c r1, r2, r3",  0, false },
        { "and",     encodeANDX(12, 10, 11, false),"and r12, r10, r11",  0, false },
        { "or",      encodeORX(1, 2, 3, false),    "or r1, r2, r3",      0, false },
        { "xor",     encodeXORX(1, 2, 3, true),    "xor.c r1, r2, r3",   0, false },
        { "neg",     encodeNEGX(1, 3, false),      "neg r1, r3",         0, false },
        { "not",     encodeNOTX(1, 2, false),      "not r1, r2",         0, false },
        { "cmp tcs0",encodeCMP(8, 9, 0),           "cmpteq.c r8, r9",    0, false },
        { "cmp tcs1",encodeCMP(8, 9, 1),           "cmptmi.c r8, r9",    0, false },
        { "cmp tcs3",encodeCMP(8, 9, 3),           "cmp.c r8, r9",       0, false },
        { "cmpz",    encodeCMPZ(4, 1),             "cmpztmi.c r4, r0",   0, false },
        { "mv{eq}",  encodeMVCOND(1, 2, EC_EQ),    "mveq r1, r2",        0, false },
        { "mv{al}",  encodeMVCOND(1, 2, EC_AL),    "mv r1, r2",          0, false },
        { "ext sb",  encodeEXTSBX(1, 2, false),    "extsb r1, r2",       0, false },
        { "ext sh",  encodeEXTSHX(1, 2, true),     "extsh.c r1, r2",     0, false },
        { "ext zb",  encodeEXTZBX(1, 2, false),    "extzb r1, r2",       0, false },
        { "ext zh",  encodeEXTZHX(1, 2, false),    "extzh r1, r2",       0, false },
    };
    for (const auto& c : cases) check32(c);
}

TEST(EncodingConformance, SpecialFormBitOps)
{
    const Case32 cases[] = {
        { "bitclr",   encodeBITCLR(5, 3, false),  "bitclr r5, 3",    0, false },
        { "bitset.c", encodeBITSET(5, 31, true),  "bitset.c r5, 31", 0, false },
        { "bittst.c", encodeBITTST(5, 0, true),   "bittst.c r5, 0",  0, false },
        { "bittgl",   encodeBITTGL(5, 7, false),  "bittgl r5, 7",    0, false },
        { "bittstc",  encodeBITTSTC(5, 7, true),  "bittst.c r5, 7",  0, false },
    };
    for (const auto& c : cases) check32(c);
}

TEST(EncodingConformance, SpecialFormShiftsAndRotates)
{
    const Case32 cases[] = {
        { "sll",   encodeSLLX(1, 2, 3, false),   "sll r1, r2, r3",    0, false },
        { "srl",   encodeSRLX(1, 2, 3, false),   "srl r1, r2, r3",    0, false },
        { "sra",   encodeSRAX(1, 2, 3, true),    "sra.c r1, r2, r3",  0, false },
        { "rol",   encodeROLX(1, 2, 3, false),   "rol r1, r2, r3",    0, false },
        { "ror",   encodeRORX(1, 2, 3, false),   "ror r1, r2, r3",    0, false },
        { "rolc",  encodeROLC(1, 2, 3, false),   "rolc r1, r2, r3",   0, false },
        { "rorc",  encodeRORC(1, 2, 3, true),    "rorc.c r1, r2, r3", 0, false },
        { "slli",  encodeSLLIX(1, 2, 5, false),  "slli r1, r2, 5",    0, false },
        { "srli",  encodeSRLIX(1, 2, 5, false),  "srli r1, r2, 5",    0, false },
        { "srai",  encodeSRAIX(1, 2, 5, false),  "srai r1, r2, 5",    0, false },
        { "roli",  encodeROLIX(1, 2, 5, true),   "roli.c r1, r2, 5",  0, false },
        { "rori",  encodeRORIX(1, 2, 5, false),  "rori r1, r2, 5",    0, false },
        { "rolic", encodeROLIC(1, 2, 5, false),  "rolic r1, r2, 5",   0, false },
        { "roric", encodeRORIC(1, 2, 5, true),   "roric.c r1, r2, 5", 0, false },
    };
    for (const auto& c : cases) check32(c);
}

TEST(EncodingConformance, SpecialFormCustomEngine)
{
    const Case32 cases[] = {
        { "mul",    encodeMUL(2, 3),         "mul r2, r3",       0, false },
        { "mulu",   encodeMULU(2, 3),        "mulu r2, r3",      0, false },
        { "div",    encodeDIV(2, 3),         "div r2, r3",       0, false },
        { "divu",   encodeDIVU(2, 3),        "divu r2, r3",      0, false },
        { "mfcel",  encodeMFCEL(4),          "mfcel r4",         0, false },
        { "mfceh",  encodeMFCEH(4),          "mfceh r4",         0, false },
        { "mfcehl", encodeMFCEHL(4, 5),      "mfcehl r4, r5",    0, false },
        { "mtcel",  encodeMTCEL(4),          "mtcel r4",         0, false },
        { "mtceh",  encodeMTCEH(4),          "mtceh r4",         0, false },
        { "mtcehl", encodeMTCEHL(4, 5),      "mtcehl r4, r5",    0, false },
    };
    for (const auto& c : cases) check32(c);
}

TEST(EncodingConformance, SpecialFormSystemAndBranchRegister)
{
    const Case32 cases[] = {
        { "syscall", encodeSYSCALL(),       "syscall 0x0000", 0, false },
        { "sdbbp",   encodeSDBBP32(),       "sdbbp 0x00",     0, false },
        { "pflush",  encodePFLUSH(),        "pflush",         0, false },
        // MAME prints the t{cond}/trap{cond} operand field loosely; check mnemonic + condition only.
        { "t{eq}",   encodeTCOND(EC_EQ),    "teq",            0, true },
        { "trap{eq}",encodeTRAPCOND(EC_EQ), "trapeq",         0, true },
        { "br r3",   encodeBR(EC_AL, 3),    "br r3",          0, false },
        { "brl r3",  encodeBRL(EC_AL, 3),   "brl r3",         0, false },
        { "br{eq}",  encodeBR(EC_EQ, 5),    "breq r5",        0, false },
        { "br{ne}l", encodeBRL(EC_NE, 5),   "brnel r5",       0, false },
        { "lcb",     encodeLCB(2),          "lcb [r2]+",      0, false },
        { "lcw",     encodeLCW(1, 2),       "lcw r1, [r2]+",  0, false },
        { "lce",     encodeLCE(1, 2),       "lce r1, [r2]+",  0, false },
        { "scb",     encodeSCB(1, 2),       "scb r1, [r2]+",  0, false },
        { "scw",     encodeSCW(1, 2),       "scw r1, [r2]+",  0, false },
        { "sce",     encodeSCE(2),          "sce [r2]+",      0, false },
    };
    for (const auto& c : cases) check32(c);
}

// ---- I-form, RI-form, LS-form ------------------------------------------------------

TEST(EncodingConformance, IFormImmediates)
{
    const Case32 cases[] = {
        { "addi -1",  encodeADDIX(4, 0xFFFF, false), "addi r4, -1",       0, false },
        { "addi.c",   encodeADDIX(4, 5, true),       "addi.c r4, 5",      0, false },
        { "andi",     encodeANDIX(12, 0xFFFF, false),"andi r12, 0xffff",  0, false },
        { "ori",      encodeORIX(28, 0x51e0, false), "ori r28, 0x51e0",   0, false },
        { "cmpi",     encodeCMPI(4, 16),             "cmpi.c r4, 0x0010",0, false },
        { "ldi",      encodeLDI(4, 1),               "ldi r4, 0x0001",    0, false },
        { "ldi neg",  encodeLDI(4, 0x8000),          "ldi r4, 0x8000",     0, false },
        { "addis",    encodeADDISX(4, 0x1234, false),"addis r4, 0x1234",  0, false },
        { "cmpis",    encodeCMPIS(4, 0x10),          "cmpis.c r4, 0x0010",0, false },
        { "andis",    encodeANDISX(4, 0xFF00, false),"andis r4, 0xff00",  0, false },
        { "oris",     encodeORISX(4, 0x00FF, true),  "oris.c r4, 0x00ff", 0, false },
        { "ldis",     encodeLDIS(28, 0xa009),        "ldis r28, 0xa009",  0, false },
    };
    for (const auto& c : cases) check32(c);
}

TEST(EncodingConformance, RIFormImmediates)
{
    const Case32 cases[] = {
        { "addri -3", encodeADDRIX(1, 2, 0x3FFD, false),  "addri r1, r2, -3",      0, false },
        { "addri.c",  encodeADDRIX(1, 2, 7, true),        "addri.c r1, r2, 7",     0, false },
        { "andri",    encodeANDRIX(12, 10, 0x234E, false),"andri r12, r10, 0x234e",0, false },
        { "orri.c",   encodeORRIX(1, 2, 0x00FF, true),    "orri.c r1, r2, 0x00ff", 0, false },
    };
    for (const auto& c : cases) check32(c);
}

TEST(EncodingConformance, LSFormAndCache)
{
    const Case32 cases[] = {
        { "lw",    encodeLW(1, 2, 8),       "lw r1, [r2, 8]",      0, false },
        { "lh",    encodeLH(1, 2, 8),       "lh r1, [r2, 8]",      0, false },
        { "lhu",   encodeLHU(1, 2, 8),      "lhu r1, [r2, 8]",     0, false },
        { "lb",    encodeLB(1, 2, 8),       "lb r1, [r2, 8]",      0, false },
        { "sw",    encodeSW(1, 2, 8),       "sw r1, [r2, 8]",      0, false },
        { "sh",    encodeSH(1, 2, 8),       "sh r1, [r2, 8]",      0, false },
        { "lbu",   encodeLBU(1, 2, 8),      "lbu r1, [r2, 8]",     0, false },
        { "sb",    encodeSB(1, 2, 8),       "sb r1, [r2, 8]",      0, false },
        { "lw -4", encodeLW(1, 2, 0xFFFC),  "lw r1, [r2, -4]",     0, false },
        { "cache", encodeCACHE(),           "cache 0x00, [r0, 0]", 0, false },
    };
    for (const auto& c : cases) check32(c);
}

TEST(EncodingConformance, RIXFormPreAndPostIndex)
{
    const Case32 cases[] = {
        { "lw pre",   encodeLW_pre(5, 6, 4),    "lw r5, [r6, 4]+",    0, false },
        { "lh pre",   encodeLH_pre(5, 6, 4),    "lh r5, [r6, 4]+",    0, false },
        { "lhu pre",  encodeLHU_pre(5, 6, 4),   "lhu r5, [r6, 4]+",   0, false },
        { "lb pre",   encodeLB_pre(5, 6, 4),    "lb r5, [r6, 4]+",    0, false },
        { "sw pre",   encodeSW_pre(2, 0, -4),   "sw r2, [r0, -4]+",   0, false },
        { "sh pre",   encodeSH_pre(5, 6, -2),   "sh r5, [r6, -2]+",   0, false },
        { "lbu pre",  encodeLBU_pre(5, 6, 1),   "lbu r5, [r6, 1]+",   0, false },
        { "sb pre",   encodeSB_pre(5, 6, 2047), "sb r5, [r6, 2047]+", 0, false },
        { "lw post",  encodeLW_post(5, 6, 4),   "lw r5, [r6]+, 4",    0, false },
        { "lh post",  encodeLH_post(5, 6, 4),   "lh r5, [r6]+, 4",    0, false },
        { "lhu post", encodeLHU_post(5, 6, 4),  "lhu r5, [r6]+, 4",   0, false },
        { "lb post",  encodeLB_post(5, 6, 4),   "lb r5, [r6]+, 4",    0, false },
        { "sw post",  encodeSW_post(5, 6, -4),  "sw r5, [r6]+, -4",   0, false },
        { "sh post",  encodeSH_post(5, 6, 2),   "sh r5, [r6]+, 2",    0, false },
        { "lbu post", encodeLBU_post(5, 6, 1),  "lbu r5, [r6]+, 1",   0, false },
        { "sb post",  encodeSB_post(10, 8, 1),  "sb r10, [r8]+, 1",   0, false },
        { "sb post -2048", encodeSB_post(1, 2, -2048), "sb r1, [r2]+, -2048", 0, false },
        // The older (d, a, imm, pre) call-site names map onto the same words.
        { "lw_idx pre",  encodeLW_idx(5, 6, 4, true),   "lw r5, [r6, 4]+", 0, false },
        { "sw_idx post", encodeSW_idx(5, 6, 4, false),  "sw r5, [r6]+, 4",  0, false },
    };
    for (const auto& c : cases) check32(c);
}

// ---- Control flow and CR-form --------------------------------------------------------

TEST(EncodingConformance, JumpsAndBranches)
{
    const Case32 cases[] = {
        { "j",       encodeJX(0x80, false),          "j 0x00000100",    0,     false },
        { "jl",      encodeJX(0x80, true),           "jl 0x00000100",   0,     false },
        { "b",       encodeBCOND(EC_AL, 4),          "b 0x00000008",    0,     false },
        { "beq back",encodeBCOND(EC_EQ, -2),         "beq 0x000000fc",  0x100, false },
        { "bnel",    encodeBCONDL(EC_NE, 16),        "bnel 0x00000120", 0x100, false },
        { "bc far",  encodeBC(EC_AL, 0x3FFFF, false),"b 0x0007fffe",    0,     false },
        { "bc far -",encodeBC(EC_AL, -0x40000, false),"b 0xfff80000",   0,     false },
    };
    for (const auto& c : cases) check32(c);
}

TEST(EncodingConformance, CRForm)
{
    const Case32 cases[] = {
        { "mtcr", encodeMTCR(4, 0),  "mtcr r4, cr0", 0, false },
        { "mfcr", encodeMFCR(4, 1),  "mfcr r4, cr1", 0, false },
        { "rte",  encodeRTE(),       "rte",          0, false },
    };
    for (const auto& c : cases) check32(c);

    // drte and sleep are not in MAME's decoder. Their CR_OP values come from the PDF Form
    // diagrams: rte 1000010 0, drte 1010010 0, sleep 1100010 0.
    EXPECT_EQ(encodeRTE(),   0x98008084u);
    EXPECT_EQ(encodeDRTE(),  0x980080A4u);
    EXPECT_EQ(encodeSLEEP(), 0x980080C4u);
}

// ---- 16-bit instructions -------------------------------------------------------------

TEST(EncodingConformance, Op0RegisterForms16)
{
    const Case16 cases[] = {
        { "nop!",    encodeNOP16(),                  "nop!",          0, false },
        { "mlfh!",   encodeMLFH16(1, 2),             "mlfh! r1, r18", 0, false },
        { "mhfl!",   encodeMHFL16(1, 2),             "mhfl! r17, r2", 0, false },
        { "mv!",     encodeMV16(1, 2),               "mv! r1, r2",    0, false },
        { "br!",     encodeBRCOND16(EC_AL, 3),       "br! r3",        0, false },
        { "br{eq}!", encodeBRCOND16(EC_EQ, 3),       "breq! r3",      0, false },
        { "t{eq}!",  encodeTCOND16(EC_EQ),           "teq!",          0, false },
        { "sll!",    encodeSLL16(1, 2),              "sll! r1, r2",   0, false },
        { "addc!",   encodeADDC16(1, 2),             "addc! r1, r2",  0, false },
        { "srl!",    encodeSRL16(1, 2),              "srl! r1, r2",   0, false },
        { "sra!",    encodeSRA16(1, 2),              "sra! r1, r2",   0, false },
        { "brl!",    encodeBRLCOND16(EC_AL, 3),      "brl! r3",       0, false },
        { "br{ne}l!",encodeBRLCOND16(EC_NE, 3),      "brnel! r3",     0, false },
    };
    for (const auto& c : cases) check16(c);
}

TEST(EncodingConformance, Op2AluAndMemoryForms16)
{
    const Case16 cases[] = {
        { "add!",  encodeADD16(1, 2),  "add! r1, r2",  0, false },
        { "sub!",  encodeSUB16(1, 2),  "sub! r1, r2",  0, false },
        { "neg!",  encodeNEG16(1, 2),  "neg! r1, r2",  0, false },
        { "cmp!",  encodeCMP16(1, 2),  "cmp! r1, r2",  0, false },
        { "and!",  encodeAND16(1, 2),  "and! r1, r2",  0, false },
        { "or!",   encodeOR16(1, 2),   "or! r1, r2",   0, false },
        { "not!",  encodeNOT16(1, 2),  "not! r1, r2",  0, false },
        { "xor!",  encodeXOR16(1, 2),  "xor! r1, r2",  0, false },
        { "lw!",   encodeLW16(1, 2),   "lw! r1, [r2]", 0, false },
        { "lh!",   encodeLH16(1, 2),   "lh! r1, [r2]", 0, false },
        { "lbu!",  encodeLBU16(1, 2),  "lbu! r1, [r2]",0, false },
        { "sw!",   encodeSW16(1, 2),   "sw! r1, [r2]", 0, false },
        { "sh!",   encodeSH16(1, 2),   "sh! r1, [r2]", 0, false },
        { "sb!",   encodeSB16(1, 2),   "sb! r1, [r2]", 0, false },
        { "pop!",       encodePOP16(1, 2, false),  "pop! r1, [r2]",  0, false },
        { "pop! high",  encodePOP16(1, 2, true),   "pop! r17, [r2]", 0, false },
        { "push!",      encodePUSH16(2, 0, false), "push! r2, [r0]", 0, false },
        { "push! high", encodePUSH16(15, 7, true), "push! r31, [r7]",0, false },
    };
    for (const auto& c : cases) check16(c);
}

TEST(EncodingConformance, JumpBranchAndImmediateForms16)
{
    const Case16 cases[] = {
        { "j!",       encodeJX16(0x10, false),        "j! 0x00000020",     0,     false },
        { "jl!",      encodeJX16(0x10, true),         "jl! 0x00000020",    0,     false },
        { "j! page",  encodeJX16(0x10, false),        "j! 0x00005020",     0x5ffe,false },
        { "b!",       encodeB16(EC_AL, 0),            "b! 0x00000000",     0,     false },
        { "bne! back",encodeBCOND16(EC_NE, -2),       "bne! 0x000000fc",   0x100, false },
        { "beq! fwd", encodeBCOND16(EC_EQ, 127),      "beq! 0x000000fe",    0,    false },
        { "ldiu!",    encodeLDIU16(6, 3),             "ldiu! r6, 0x03",    0,     false },
        { "ldiu! ff", encodeLDIU16(15, 0xFF),         "ldiu! r15, 0xff",   0,     false },
        { "addei!",   encodeADDEI16(1, 5),            "addei! r1, 5",      0,     false },
        { "subei!",   encodeSUBEI16(1, 5),            "subei! r1, 5",      0,     false },
        { "slli!",    encodeSLLI16(1, 3),             "slli! r1, 3",       0,     false },
        { "sdbbp!",   encodeSDBBP16(0, 2),            "sdbbp! r0, 2",      0,     false },
        { "srli!",    encodeSRLI16(1, 3),             "srli! r1, 3",       0,     false },
        { "bitclr!",  encodeBITCLR16(1, 3),           "bitclr! r1, 3",     0,     false },
        { "bitset!",  encodeBITSET16(1, 3),           "bitset! r1, 3",     0,     false },
        { "bittst!",  encodeBITTST16(1, 3),           "bittst! r1, 3",     0,     false },
        { "bittgl!",  encodeBITTGL16(1, 3),           "bittgl! r1, 3",     0,     false },
    };
    for (const auto& c : cases) check16(c);
}

TEST(EncodingConformance, BPRelativeForms16)
{
    // `imm5` is the raw field; the disassembler prints the scaled byte offset.
    const Case16 cases[] = {
        { "lwp!",  encodeLWP16(1, 3),  "lwp! r1, 12", 0, false },
        { "lhp!",  encodeLHP16(1, 3),  "lhp! r1, 6",  0, false },
        { "lbup!", encodeLBUP16(1, 3), "lbup! r1, 3", 0, false },
        { "swp!",  encodeSWP16(1, 3),  "swp! r1, 12", 0, false },
        { "shp!",  encodeSHP16(1, 3),  "shp! r1, 6",  0, false },
        { "sbp!",  encodeSBP16(1, 3),  "sbp! r1, 3",  0, false },
    };
    for (const auto& c : cases) check16(c);
}
