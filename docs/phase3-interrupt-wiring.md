# Phase 3 — Interrupt / Exception Hardware Wiring: Implementation Design

Status: implemented; the full suite (428 tests) passes. See section 8 for deviations from this design and findings.
Depends on: Phase 1 (byte-addressed bus, done). Benefits from Phase 2 (CR-form opcode fixes, which this phase pulls forward).
Feeds: Phase 4 (frame loop generates the vblank events), Phase 5 (illegal-instruction trap reuses the exception entry path).

## 1. Sources and confidence

| Source | Used for |
|---|---|
| `s_core7.pdf` (S+core7 Programming Guide v1.4) | `rte`/`syscall`/`trap`/`sleep`/`drte` semantics, `cr0..cr31` names, exc_code 7 = syscall, 10 = trap. **Instruction-set only: it has no chapter on PSR/ECR bit layout, vector offsets or the interrupt controller.** |
| SPG290 Programming Guide v0.5 (public copy at `LiraNuna/hyperscan-emulator/docs/spg290.pdf`) | SoC memory map, interrupt controller (ch. 26), PPU IRQ registers, source numbering |
| Linux 4.0 `arch/score/` (Sunplus-authored; `irq.h`, `irq.c`, `entry.S`, `traps.c`, `asmmacro.h`, `irqflags.h`) | PSR.IE bit, vector layout, ECR fields, exception-code table, controller register offsets and mask polarity |
| `AloysHF/HyperScanEmu` `docs/Hardware.md` | Cross-check: controller regs at `0x080A_0000..0x24`, PPU vblank = IRQ 53, level-sensitive, priority arbitration still provisional |

Confidence tags used below: **[doc]** stated in a manual, **[linux]** observed in Sunplus's own Linux port, **[inferred]** deduced from the two, **[unknown]** must be settled by real-ROM bring-up.

## 2. Corrections to the original Phase 3 plan

1. **`status.I` is the wrong gate.** The real interrupt-enable bit is **PSR.IEc, bit 0 of `cr0`** [linux: `ori r8,1` / `mtcr r8,cr0` enables IRQs]. The NES-6502-style `status` flags (`I`, `D`, `B`, `U`) have no hardware meaning and should be removed rather than wired.
2. **There are two entry points, not one.** Software/synchronous exceptions (syscall, trap, RI, address error...) go to a *general* vector; hardware IRQs go to a separate *interrupt* vector chosen by IRQ number [linux]. The current single `pc = cr[3]` is wrong for both.
3. **EXCPvec (`cr3`) is a base plus a mode bit, not a jump target.** Linux writes `0xA0000000 | 1` [linux]: bit 0 selects 16-byte vs 4-byte vector spacing.
4. **The CPU does not arbitrate.** The Score7 core has one IRQ input (plus NMI/debug). Priority lives in the SoC interrupt controller, which also supplies the vector number [doc ch. 26].
5. **Bus address aliasing is a hard prerequisite.** Real firmware programs EXCPvec with a `0xA000_0000`-based address and the PPU is documented at both `0x0801_0000` and `0x8801_0000`. The current `Bus` only maps `0..256 KiB` and `PPU::BASE`; an access at `0xA0000000` reads 0 and writes are dropped, so no handler could ever be reached. See 3.0b.

## 3. Hardware model to implement

### 3.1 CPU-side architectural state

| Register | Fields used by Phase 3 | Confidence |
|---|---|---|
| `cr0` PSR | `IEc` bit 0 (current IRQ enable). Proposed layout from the `rte` formula `PSR.{UMs,IEs,UMc,IEc}` read high-to-low: `UMs`=bit 3, `IEs`=bit 2, `UMc`=bit 1, `IEc`=bit 0. Linux confirms bit 0 = IE and bit 3 = "previous user mode" (`user_mode()` tests `psr & 8`, and `SAVE_ALL` shifts bit 3 into the sign). Bits 1 and 2 are inferred. | bit 0, bit 3 [linux]; bits 1, 2 [inferred] |
| `cr1` Condition | `Tc,Nc,Zc,Cc,Vc` and saved `Ts,Ns,Zs,Cs,Vs` [doc]; bit positions [unknown]. | Names only |
| `cr2` ECR | `Exc_code` in bits 4:0 (`andi r31,0x1f`) [linux]. `IP[7:2]` (the vectored IRQ number, 0..63) is read as `ecr >> 18` [linux], so `IP` sits at bits 23:16 with `IP[7:2]` = bits 23:18 [inferred]. `IM` field exists [doc, `sleep`] but position [unknown]. | partly [linux] |
| `cr3` EXCPvec | bits 31:1 base, bit 0 = spacing mode (1 = 16 bytes, 0 = 4 bytes) | [linux] |
| `cr5` EPC | Linux clears bit 0 when saving/restoring EPC, so bit 0 may carry state; mask it on `rte`. | [linux] |
| `cr30` DEPC | Already used by `drte`. Debug exceptions stay out of scope. | [doc] |

Reset values: `cr0 = 0` (kernel mode, interrupts disabled). All other control registers 0.

### 3.2 Vector addresses [linux, inferred for mode 4]

```
base    = cr3 & ~1
spacing = (cr3 & 1) ? 0x10 : 0x04
debug   = base + 0x1FC
general = base + 0x200                  // syscall, trap, RI, address/bus errors, ...
irq(n)  = base + 0x200 + spacing*(n+1)  // n = vector number 0..63, i.e. IP[7:2]
```

For mode 16 this gives `0x210 + 0x10*n`, matching Linux's `IRQ_VECTOR_BASE_ADDR = base+0x210` and `..END = base+0x5F0`. The mode-4 formula is a straight extrapolation and must be confirmed against the BIOS. The PPU vblank (vector 53) lands at `base + 0x550` in mode 16.

Exception code table (ECR.Exc_code) [linux `traps.c`]: 1 NMI, 2 AdEL-insn, 3/4 TLB (insn), 5 IBE, 6 privilege (PEL), **7 syscall**, 8 CCU, 9 RI, **10 trap**, 11/12 AdEL/AdES-data, 13/14/15 TLB (data), 16 CeE, 17 CpE, 18 DBE. Only 7 and 10 exist today; add the rest as named constants so Phase 5 can raise RI (9) and CeE (16, divide-by-zero).

### 3.3 Exception entry and return

Common entry (all exceptions and IRQs), in this order:

1. `PSR.UMs = PSR.UMc; PSR.IEs = PSR.IEc; PSR.UMc = 0 (kernel); PSR.IEc = 0` [doc: `rte` is the inverse; syscall/trap text says IEc/KUc "are saved"].
2. `Condition.{Ts..Vs} = {Tc..Vc}` (save flags) [doc].
3. `ECR.Exc_code = code` (leave `IM`/`IP` untouched for software exceptions); for IRQ, set `ECR.IP[7:2] = vector`.
4. `EPC`:
   * synchronous exceptions: address of the faulting instruction = `pc - instrSize` (matches the PDF "EPC points at the syscall instruction" and today's `raiseException`);
   * hardware IRQ: address of the **next instruction to execute** (the interrupt is taken between instructions, `pc` as-is) [inferred; Linux `ret_from_irq` does a plain `rte` without adjusting EPC, whereas `handle_sys` explicitly does `EPC += 4` to skip the syscall].
5. `pc = general` or `irq(n)` per 3.2.

`rte` [doc]: `PSR.{UMc,IEc} = PSR.{UMs,IEs}`; `Condition.{Tc..Vc} = {Ts..Vs}`; `pc = EPC & ~1`. `UMs/IEs` are left as they are (the formula writes only the current pair).

The emulator's `status` flags (C, Z, N, V, T) stay the working copy of the Condition bits. Add a private `condSaved` word that entry fills and `rte` drains; this keeps the shadow bits private until `cr1` bit positions are known. Make `mfcr/mtcr cr1` read/write `status` so Linux-style save/restore of `cr1` round-trips. Document that cr1's bit layout is emulator-internal for now.

### 3.4 CPU interrupt sampling

* New CPU input: `void setIRQ(bool level, uint8_t vector)`. It only stores `irqLevel`/`irqVector`; no side effects.
* In `clock()`, **when `cycles == 0` and before fetch**, if `irqLevel && (cr[0] & IEc)` then perform the 3.3 entry with `irq(irqVector)` and return (the entry consumes this clock). One check per instruction boundary; level-sensitive, so if the handler returns without clearing the source it re-enters immediately, as hardware would.
* **ECR.IM**: not modelled initially (position unknown). Add a single `bool irqMasked()` seam that currently returns `false`, so it can be filled in once firmware is seen writing `IM`.
* `sleep` (`cr` opcode 0x06, func6 0x22): set `halted = true`. While halted, `clock()` does nothing except re-check the IRQ line. Wake on line assertion; take the interrupt only if `IEc` is set, otherwise resume at the next instruction [unknown; the PDF only says the clock is held until an interrupt is received].
* Privilege (`cra` bit, CCU/PEL exceptions) stays out of scope; the BIOS and games run in kernel mode.

### 3.5 Interrupt controller (new device, `src/IRQController.h/.cpp`)

Window `0x880A0000` (map table: `0x080A_0000..0x080A_FFFF`, "IRQCTL") [doc]. Register offsets from the doc plus Linux's `INT_MASKL/H` [linux]; HyperScanEmu independently places "pending, software interrupt, priority and mask" in `0x080A_0000..0x24`.

| Offset | Name | Access | Behaviour |
|---|---|---|---|
| 0x00 | `P_INTPND` | R | bit *n* = source *n* line level (sources 0..31) [doc] |
| 0x04 | `P_INTPND_H` | R | bits 7:0 = sources 32..39 [doc] |
| 0x08 | `P_I_PMST` | R/W | 2 bits per slave group (0..3) = group priority, `00` = highest [doc] |
| 0x0C | `P_I_GPUSWI` | R/W | write bit 0 = 1 asserts the GPU software interrupt (source 10 / vector 53); bit 8 = 1 clears it [doc] |
| 0x10..0x1C | `P_I_PSLV0..3` | R/W | 3 bits per source within a group, source *k* at bits `3k+2:3k`, `000` = highest [doc] |
| 0x20 | `P_I_MASKL` | R/W | bit *n* = 1 masks source *n* (sources 0..31) [linux; polarity: 1 = masked] |
| 0x24 | `P_I_MASKH` | R/W | same for sources 32..39 [linux] |

Source number to vector: `vector = 63 - source` (Linux `63 - irq`; doc table 26.5). Group = `source / 8`.

Behaviour:
* Inputs are **level-sensitive, active high** [doc]: `setSource(n, level)` drives a bit; the pending register reflects the raw level (assumption: pending shows the unmasked level, since Linux never reads it).
* Reset: `mask = 0` per the hardware default is [unknown]; Linux writes `0xFFFFFFFF` first. Reset to **all masked** so a ROM that forgets to initialise is safe.
* Output each time an input, mask or priority changes: among `pending & ~mask` choose the winner by (group priority from `PMST`, then source priority from `PSLV`, then **lowest source number** i.e. highest vector). The last tie-break matches HyperScanEmu's provisional rule. Expose `bool irqActive()` and `uint8_t activeVector()`.
* Implemented sources for Phase 3: **10 PPU vblank start (vec 53)**, **17 PPU H/V hit (vec 46)**, **36 TV/PPU vblank end (vec 27)**, software interrupt on 10. All other sources are present as inputs but nothing drives them. Source table 0..39 should be a `constexpr` array of names for diagnostics.

### 3.6 PPU interrupt sources

PPU registers already exist as raw storage (`P_IRQ_Control` 0x80, `P_IRQ_Status` 0x84, `P_IRQTMV` 0x88, `P_IRQTMH` 0x8C). Bit layout [doc ch. 9]:

* Control: bit 0 `BLK_ST_EN`, bit 1 `BLK_END_EN`, bit 2 `HV_IRQ_EN`.
* Status: bit 0 `BLK_IRQ_ST`, bit 1 `BLK_IRQ_END`, bit 2 `HV_IRQ`.

Changes in `ppu.h/.cpp`:
* Named constants for those bits.
* Event hooks, called by whoever owns timing: `signalVBlankStart()`, `signalVBlankEnd()`, `signalHVHit()`. Each sets its status bit **only if the matching enable bit is set** [inferred; the guide describes "status: occurred"; revisit if firmware polls status with the enable off].
* Lines: `blkStartLine() = status.BLK_ST && ctrl.BLK_ST_EN` and likewise for the other two, so disabling the enable drops the line.
* `write32(P_IRQ_Status)`: **write-1-to-clear** [inferred; the guide does not say, but a plain RW status register would make the handler's ack impossible]. Everything else in `m_ctrl` keeps current behaviour. This is the first register with read/write asymmetry, so special-case it explicitly rather than in the generic `off < 0x1000` path.
* `Bus::write8/write16` currently implement sub-word writes as read-modify-write of the word. For a W1C register that would clear unrelated status bits. For the IRQ status and `P_I_GPUSWI` registers, build the word from the written lane only (other lanes zero).

Timing itself (scanline counter, `P_IRQTMV/H` comparison, `P_VBLK_TIME`) belongs to Phase 4; Phase 3 only provides the hooks plus tests that call them directly.

### 3.7 Bus integration

* Add `IRQController irqc;` to `Bus`, decoded at `IRQController::BASE = 0x880A0000`, 64 KiB window.
* Add one function, `Bus::updateIRQ()`:
  ```
  irqc.setSource(10, ppu.blkStartLine() || irqc.gpuSwi());
  irqc.setSource(17, ppu.hvLine());
  irqc.setSource(36, ppu.blkEndLine());
  cpu.setIRQ(irqc.irqActive(), irqc.activeVector());
  ```
* Call it after any write to the PPU or IRQC windows and after any PPU event hook. Do not call it per instruction. A cheap way is a `dirty` flag set by those paths and consumed once per `Bus::tick()`; the unit tests can simply call `updateIRQ()` directly.
* **Address aliasing (prerequisite 3.0b)**: decode on `phys = addr & 0x1FFFFFFF`. That maps `0xA0000000 -> 0x0` (RAM, where Linux and the BIOS put vectors), `0x88010000 -> 0x08010000` (PPU) and `0xBF000000 -> 0x1F000000` (boot ROM), consistent with the SoC map. Keep the PPU/IRQC constants in whichever form the code already uses and compare after masking. The PPU register access path stays unchanged (it already accepts both absolute and BASE-relative forms).

## 4. Implementation steps

Each step ends with `make -f build.mk test` green. Remember the gotcha: `make -f build.mk clean` first whenever only `tests/test_helpers.h` changed.

### 3.0a Real CR-form decode (prerequisite, pulled forward from the Phase 2 deferred list)

Probed with `score-elf-as -SCORE7` / `score-elf-objdump` (raw words as printed by objdump; fields via the usual shifts):

| Instruction | opcode | rD | rA | func6 | CU |
|---|---|---|---|---|---|
| `mfcr rD, crN` | 0x06 | rD | N | 0x00 | **1** |
| `mtcr rD, crN` | 0x06 | rD (source GPR) | N | 0x00 | **0** |
| `rte` | 0x06 | 0 | 0 | 0x02 | 0 |
| `drte` | 0x06 | 0 | 0 | 0x12 | 0 |
| `sleep` | 0x06 | 0 | 0 | 0x22 | 0 |

(`syscall imm15` is already R-type func6 0x01. `mfcr r4,cr0` = `0x98808001`, `mtcr r4,cr0` = `0x98808000`, `rte` = `0x98008084`.)

Work: add `OP_CRFORM = 0x06` dispatch in `clock()` with func6/CU sub-decode; route to existing `MFCR/MTCR/RTE/DRTE`; add `SLEEP`. Leave the old `OP_SYS` path in place for the not-yet-migrated instructions (mfcx etc.) so this step stays small. Update `encodeMFCR/MTCR/RTE/DRTE` in `tests/test_helpers.h`, and add the three real words above as decode tests. This is also the encoding the ROM test in 3.6 will be assembled with.

### 3.0b Bus address aliasing

Mask to `0x1FFFFFFF` in `Bus::read8/16/32` and `write8/16/32` region dispatch. Add tests: write at `0`, read at `0xA0000000`; PPU register reachable at both `0x08010000` and `0x88010000`. Existing tests should be unaffected (they use addresses below 1 MiB).

### 3.1 Exception entry/return model

1. Define PSR/ECR masks and `EXC_*` constants in `spg290.h`; drop `FLAGS290::I/D/B/U`.
2. Replace `raiseException(code)` with `enterException(code, epc, vectorAddr)` implementing 3.3; add `generalVector()` / `irqVector(n)` helpers per 3.2.
3. Rewrite `RTE()` per 3.3. Keep `DRTE()` as is.
4. Make `cr1` access map to `status`; add `condSaved`.
5. Update tests in `tests/test_trap_system_instructions.cpp`: they currently set `cr[3] = 0x40` and expect `pc == 0x40`. New expectations: with `cr[3] = 0x41` (mode 16) the general vector is `0x240`; `cr[2] & 0x1F` is 7 or 10; `cr[5]` is the faulting address; `PSR.IEc` is cleared with `IEs` holding the old value; `rte` restores them and the flags. Add a nested-exception test documenting that `IEs` is overwritten (single-level shadow, no stack).

### 3.2 CPU IRQ input and `sleep`

1. Add `setIRQ()`, `irqLevel`, `irqVector`, `halted`, and the pre-fetch check in `clock()`.
2. Tests (use `CPUFixture`, no controller needed):
   * line high + `IEc=0`: instruction executes normally, no entry;
   * line high + `IEc=1`: next `clock()` enters at `irq(vector)`, `ECR.IP[7:2] == vector`, `EPC` is the not-yet-run instruction, `IEc` cleared;
   * `rte` returns and the interrupted instruction then executes exactly once;
   * line still high after `rte` re-enters (level-sensitive);
   * `sleep` halts: `pc` does not advance across several `clock()` calls until `setIRQ(true, ...)`;
   * both vector modes (`cr3` bit 0 = 0 and 1).

### 3.3 Interrupt controller

1. Add `IRQController` with the register file in 3.5, `setSource`, `irqActive`, `activeVector`, `read32/write32`, `reset`.
2. Wire into `Bus` decode (3.7) including sub-word write handling.
3. Unit tests (`tests/test_irq_controller.cpp`, no CPU): pending reflects inputs; mask suppresses; vector = 63 - source; priority by `PMST` and `PSLV`; tie-break; software interrupt set/clear; `PND_H` covers sources 32..39; mask/priority registers round-trip; reset state is all masked.

### 3.4 PPU sources

1. Add constants, the three event hooks, line accessors, and W1C on `P_IRQ_Status` (3.6).
2. Tests in `tests/test_ppu_registers.cpp` style: event with enable off leaves status clear; event with enable on sets only its bit; line drops after W1C; writing 0 to the status register does nothing; a byte write to one lane of status does not clear bits in other lanes (through `Bus::write8`).

### 3.5 Bus glue and system test

1. Implement `Bus::updateIRQ()` per 3.7 and the dirty-flag call sites.
2. `tests/test_irq_integration.cpp`: PPU enable + controller unmask + `IEc=1`, fire `signalVBlankStart()`, call `updateIRQ()`, `clock()`; expect entry at `base + 0x550` (mode 16), `ECR.IP[7:2] == 53`. Then mask the source and confirm no entry. Then raise the line while `IEc=0`, set `IEc` through `mtcr`, and confirm entry occurs on the following instruction.

### 3.6 Tier-2 GAS ROM test

Add `tests/roms/test_irq.s` (same pipeline as `test_and.s`; regenerate the header with `make -f build.mk roms`):

1. `mtcr` EXCPvec = `0xA0000001`; install a `j handler` at `base + 0x550` (and at `base + 0x200` for a trap handler).
2. Unmask source 10 via `P_I_MASKL`, enable PPU `BLK_ST_EN`, set `PSR.IEc` with `mfcr/ori/mtcr`.
3. Spin until a counter in RAM reaches 3.
4. Handler: increment counter, write `1` to `P_IRQ_Status` bit 0 (ack), `rte`.
5. Write `PASS_VALUE` / done flag as in the existing ROM protocol.

The fixture injects the vblank by calling `ppu.signalVBlankStart(); bus.updateIRQ();` every N clocks. A second variant executes `syscall` and `trapeq` and checks the general-vector path plus `ECR.Exc_code`. Because GAS produced these encodings, this is the strongest evidence the CR-form decode, vector layout and `rte` agree with real code. Expect the assembler to warn about "data dependency ... bubble" after `mtcr`; either pad with `nop`s as Linux does, or ignore, since the emulator applies CR writes immediately.

## 5. Interface with Phase 4

Phase 4 owns time. For Phase 3 to be usable it needs these constants and one entry point, but none of them need to be decided now:

* Entry point: `Bus::tick(uint32_t cpuCycles)` (advance a scanline counter, call the three PPU hooks, then `updateIRQ()` if anything changed).
* Starting values (tunable, label them as placeholders): HyperScanEmu documents a 27 MHz pixel clock, NTSC 858x525 total, giving about 59.94 Hz, and ppcasm reports the retail CPU at 108 MHz. That is 4 CPU cycles per pixel clock, **3432 cycles/line and about 1,801,800 cycles/frame**. This emulator charges one cycle per instruction while HyperScanEmu uses a six-cycle estimate, so decide a `CYCLES_PER_INSTRUCTION` constant there and scale the frame period by it rather than shortening the frame.
* `P_IRQTMV/H` comparison and `P_VBLK_TIME` are consumed by that scanline counter.

## 6. Risks and open questions

1. **PSR bits 1 and 2 and the ECR `IM`/`IP` positions are inferred.** They matter only to firmware that reads them directly. Mitigation: the layout lives in one set of named masks; adjust on first contradicting real-ROM trace.
2. **Vector mode 4 and the exact irq offset formula** are extrapolated from Linux's mode-16 layout. If the BIOS sets `cr3` bit 0 = 0, confirm with a trace of the first interrupt.
3. **Endianness conflict.** SPG290 guide section 3.1 states all SPG290 system accesses are little-endian, while Phase 1 stored RAM big-endian to match the toolchain's default `-EB` output. Word-wide MMIO such as the IRQ controller is unaffected, but byte and half-word lanes in `Bus::write8/16` (used by the W1C handling above) depend on this. Resolve before Phase 4 loads real ROM dumps; if the bus flips to little-endian this document's byte-lane rules flip with it.
4. **Status-register W1C and "status sets only when enabled"** are assumptions. They are isolated in two small PPU functions, so changing them is trivial if the BIOS disagrees.
5. **Controller mask reset value and pending-register masking** are unspecified. The all-masked reset is the safe choice.
6. **EPC for interrupts** (next instruction vs interrupted instruction) is inferred from Linux behaviour. A wrong choice either skips or double-executes one instruction on return; the integration test in 3.5 asserts the "executes exactly once" property.
7. **NMI, debug exceptions, single-step, TLB/privilege exceptions** are deliberately not modelled. The entry function takes a generic vector and exception code so they can be added without redesign.
8. **CE exception for divide-by-zero** (`CeE`, exc_code 16 per the PDF) and **RI** can reuse `enterException` as soon as Phase 5 replaces the silent default cases.

## 7. File-level change summary

| File | Change |
|---|---|
| `src/spg290.h/.cpp` | PSR/ECR masks, `EXC_*` codes, `enterException`, new `RTE`, `SLEEP`, CR-form dispatch, `setIRQ`, `halted`, `condSaved`, remove `I/D/B/U` flags |
| `src/IRQController.h/.cpp` (new) | Controller register file and arbitration |
| `src/ppu.h/.cpp` | IRQ bit constants, event hooks, line accessors, W1C on `P_IRQ_Status` |
| `src/Bus.h/.cpp` | `irqc` member, address aliasing, window decode, lane-only writes for W1C registers, `updateIRQ()` |
| `src/build.mk` | Add an explicit `IRQController.o` rule and link it into `main` and `test_runner` (rules are hand-written per object, not wildcarded; test `.cpp` files are wildcarded) |
| `tests/test_helpers.h` | Real CR-form encoders; ROM fixture hook to inject PPU events |
| `tests/test_trap_system_instructions.cpp`, `tests/test_system_instructions.cpp` | Update to new vector/PSR semantics and CR-form encodings |
| `tests/test_irq_controller.cpp`, `tests/test_irq_integration.cpp` (new) | Controller, CPU IRQ, and end-to-end tests |
| `tests/roms/test_irq.s` + `generated/test_irq.h` (new) | Tier-2 GAS ROM test |

## 8. Implementation notes (post-implementation)

Deviations from the design above:

* **No dirty flag.** `Bus::updateIRQ()` is called directly after every PPU/IRQC MMIO write (and by tests after raising a PPU event hook). It is four `setSource` calls and one `setIRQ`, so a deferred flag was unnecessary. `Bus::tick()` is left to Phase 4.
* **Software interrupt lives inside `IRQController`.** `P_I_GPUSWI` raises source 10 internally (`pending()` ORs it in), so `Bus::updateIRQ()` only mirrors the three PPU lines.
* **`halted` is cleared by `enterException`**, so a synchronous exception always wakes the core.
* **ROM test** (`tests/roms/test_irq.s`) uses `lw_e`/`sw_e`/`j_e` from `emul_score7.inc` plus native `li`, `ldi`, `ori`, `mfcr`, `mtcr`, `syscall`, `sub.c`, `bne` (short backward only). It uses only r16+ with small immediates so GAS never emits 16-bit `!` forms (`ldi r12,1`, `sw r11,[r12]`, `mv`, `nop` all became 16-bit for low registers). The harness hook is `ROMFixture::runROMWithVBlank()`. The C++ test checks the counter (>= 3) and the saved `ECR.Exc_code == 7`.
* **Tests added:** `test_irq_cpu.cpp` (CPU sampling, vector modes, rte, sleep, real GAS CR-form words), `test_irq_controller.cpp`, `test_irq_integration.cpp` (aliasing, PPU W1C, byte-lane write, end-to-end), `test_rom_irq.cpp`. `test_trap_system_instructions.cpp` was updated to the new vector/PSR semantics.

Pre-existing decode discrepancies the GAS ROM exposed (not fixed in this phase; real GAS output is decoded wrongly, emulator-encoded tests pass because encoder and decoder share the bug):

1. **Load/store immediate.** `LW/SW/...` read `(instr >> 1) & 0x7FFF`, but the real Imm15 is bits 14:0 and bit 15 is the P-bit. A GAS `lw r21,[r20,4]` (`0xc2b48004`) decodes as a large negative offset.
2. **`j`/`jl` displacement.** `JX()` reads bits 24:1, which includes the P-bit at bit 15; real J-form excludes it.
3. **`b{cond}` upper displacement.** `decodeBCondDisp19()` takes `Disp[18:9]` from bits 24:15; the real field is bits 25:16 with bit 15 as the P-bit. Short backward branches happen to work because the sign bits make both readings agree; forward branches are wrong (GAS `bne` forward by 8 halfwords landed at `pc + 0x410`).

These belong to the Phase 2 audit; fix them together with `encLS`/`encodeBC`/`encodeJX` in `tests/test_helpers.h` and the `sw_e`/`lw_e`/`j_e`/`beq_e` macros, then regenerate `tests/roms/generated/*.h`.
