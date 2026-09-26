# UltraScan-DF5 — HyperScan / SPG290 Emulator

This project is a work-in-progress emulator for the **Sunplus SPG290**
processor used in the **HyperScan** video game console.  The CPU core
implements the **S+core7 (Score7)** 32-bit RISC architecture.

---

## Repository layout

```
src/               Emulator source code
  spg290.h / .cpp  CPU: registers, instruction dispatch, instruction implementations
  Bus.h / .cpp     Memory bus (64K × 32-bit word RAM)
  main.cpp         Manual smoke-test entry point
  build.mk         All make targets (build / test / roms)

tests/             Google Test unit test suite
  test_helpers.h   Shared fixture (CPUFixture, ROMFixture) and instruction encoders
  test_*.cpp       One file per instruction family
  test_rom_smoke.cpp   Tier-1 hand-coded ROM test
  test_rom_and_gas.cpp Tier-2 GAS-assembled ROM test
  roms/            GAS assembly sources for ROM tests
    emul_score7.inc    Compatibility macros (delegates to real mnemonics where possible)
    score.ld           Bare-metal linker script (code at address 0x0000)
    test_and.s         Example ROM: exercises ANDX end-to-end
    generated/         Auto-generated C++ headers (committed — CI does not need the toolchain)
      test_and.h

tools/
  bin_to_cpp.py    Converts an assembled binary to a uint32_t[] C++ header
```

---

## Building and running the unit tests

### Prerequisites

| Tool | Install command |
|------|----------------|
| g++ (C++17) | `sudo apt-get install build-essential` |
| Google Test  | `sudo apt-get install libgtest-dev` |

### Run all tests

All make commands must be run from the `src/` directory:

```bash
cd src
make -f build.mk test
```

This compiles the emulator, compiles every `tests/*.cpp` file, links them
against gtest, and runs the test binary.  Expected output:

```
[==========] 322 tests from 2 test suites ran.
[  PASSED  ] 322 tests.
```

### Run a specific test or filter by name

```bash
cd src
../tests/test_runner --gtest_filter="CPUFixture.ANDX*"
../tests/test_runner --gtest_filter="ROMFixture.*"
```

### Clean build artefacts

```bash
cd src
make -f build.mk clean
```

---

## Test structure

### Tier 1 — unit tests (`CPUFixture`)

Each test creates a fresh `Bus` (zeroed RAM, CPU connected), encodes a single
instruction word using the helper functions in `test_helpers.h`, executes one
clock cycle, and checks register values and flags.

```cpp
TEST_F(CPUFixture, ANDX_BasicResult)
{
    setReg(10, 0xF0F0F0F0u);
    setReg(11, 0xFF00FF00u);
    execute(encodeANDX(12, 10, 11, false));
    EXPECT_EQ(getReg(12), 0xF000F000u);
}
```

### Tier 1 — ROM smoke tests (`ROMFixture`)

A self-contained `uint32_t[]` program is loaded at address 0, clocked until
it writes to `ROM_DONE_ADDR` (0xFF00), and the result at `ROM_RESULT_ADDR`
(0xFF01) is checked against `PASS_VALUE` (0xDEAD).  This exercises multiple
instructions together including branches, stores, and the load-immediate
pattern.

### Tier 2 — GAS-assembled ROM tests (`ROMFixture`)

The same `ROMFixture` infrastructure is used, but the `uint32_t[]` array is
generated from a real Score7 assembly source file (`tests/roms/test_and.s`)
assembled with the Score cross-toolchain.  The generated header
(`tests/roms/generated/test_and.h`) is committed so the tests run on any
machine without the toolchain installed.

---

## Rebuilding the GAS-assembled ROM headers

The generated headers in `tests/roms/generated/` are already committed and do
**not** need to be regenerated to run the tests.  Regenerate only when you
modify a `.s` source file.

### Prerequisites — Score cross-toolchain (one-time setup)

The Score7 architecture was removed from mainline GNU binutils around 2017.
You must build binutils 2.25 from source:

```bash
# Download and extract
wget https://ftp.gnu.org/gnu/binutils/binutils-2.25.1.tar.gz
tar xf binutils-2.25.1.tar.gz

# Configure for Score ELF target
mkdir binutils-build && cd binutils-build
/path/to/binutils-2.25.1/configure \
    --target=score-elf \
    --prefix=/usr/local/score-elf \
    --disable-nls \
    --disable-werror

# Build and install (takes ~10 minutes)
make -j$(nproc)
sudo make install
```

This installs `score-elf-as`, `score-elf-ld`, and `score-elf-objcopy` to
`/usr/local/score-elf/bin/`.  If you install to a different prefix, update
the `SCORE_AS`, `SCORE_LD`, and `SCORE_OBJCOPY` paths at the top of the
`roms` section in `src/build.mk`.

### Regenerate headers

```bash
export PATH="$PATH:/usr/local/score-elf/bin"
cd src
make -f build.mk roms
```

The pipeline for each `.s` file is:

```
.s  →  score-elf-as  →  .o
.o  →  score-elf-ld  →  .elf   (linker script: .text at address 0)
.elf → score-elf-objcopy → .bin  (flat big-endian binary)
.bin → tools/bin_to_cpp.py → .h  (uint32_t[] C++ header)
```

The `.o`, `.elf`, and `.bin` intermediates are deleted automatically after
the `.h` is produced.

### Adding a new GAS ROM test

1. Write `tests/roms/my_test.s`.  Use real Score7 mnemonics where possible.
   For instructions whose LS/branch format still differs from the emulator's
   decoder, use the macros in `tests/roms/emul_score7.inc`.
2. Run `make -f build.mk roms` to generate `tests/roms/generated/my_test.h`.
3. Write a gtest file `tests/test_rom_my_test.cpp` that includes the header
   and calls `runROM(rom_my_test, rom_my_test_len)`.
4. Commit both the `.s` source and the generated `.h`.

---

## ROM protocol

All ROM programs (both hand-coded and GAS-assembled) follow this contract so
that `ROMFixture` can detect pass/fail without a debugger:

| Address | Purpose |
|---------|---------|
| `0xFF00` (`ROM_DONE_ADDR`) | ROM writes any non-zero value here when done |
| `0xFF01` (`ROM_RESULT_ADDR`) | ROM writes `0xDEAD` on pass, an error code on fail |

The `ROMFixture::runROM()` helper clocks the CPU until `ROM_DONE_ADDR` is
non-zero (or a maximum cycle count is reached), then checks
`ROM_RESULT_ADDR`.

---

## Known encoding gaps

Several instructions have placeholder func6/opcode values because their real
Score7 encoding has not yet been confirmed from a GAS disassembly:

- `CMP`, `CMPZ` — register comparison (real func6 unknown)
- `MVCOND` — conditional move
- `REM`, `REMU` — remainder
- `ROLC`, `ROLIC`, `RORC`, `RORIC` — rotate through carry
- `SUBRIX` — subtract register+immediate (GAS may fold this into `addri`)
- Load/store immediate format — the emulator uses bits 15:1 for the offset;
  real GAS uses a non-contiguous layout.  `sw_e` / `j_e` macros in
  `emul_score7.inc` emit `.long` with the emulator's format until this is fixed.

To find the real encoding for any of these, assemble a small test with
`score-elf-as`, disassemble with `score-elf-objdump -d`, and extract
`(word & 0x7C000000) >> 26` (opcode) and `(word & 0x7E) >> 1` (func6).
Then update the matching `F6_*` / `OP_*` constant in `src/spg290.h` and the
corresponding `EF6_*` constant in `tests/test_helpers.h`.
