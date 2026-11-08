# Third-party notices

## MAME S+core7 disassembler (`tools/sdis/`)

The following files are copied unmodified from the MAME project
(<https://github.com/mamedev/mame>), commit `b96603b6ba791faf0a9cc5b8f578a0f180d02a46`,
directory `src/devices/cpu/score/`. Each file keeps its original header
(`license:BSD-3-Clause`, `copyright-holders:Sandro Ronco`).

| File | Author | License |
|---|---|---|
| `tools/sdis/scoredsm.cpp` | Sandro Ronco | BSD-3-Clause |
| `tools/sdis/scoredsm.h` | Sandro Ronco | BSD-3-Clause |
| `tools/sdis/scorem.h` | Sandro Ronco | BSD-3-Clause |

The other files in `tools/sdis/` (`emu.h`, `score_disasm.*`, `main.cpp`) were written for this
project and are covered by the repository license (GPL-3.0). `emu.h` is a minimal stand-in for
MAME's `emu.h` and contains no MAME code.

## Peripheral behaviour derived from MAME (`src/Timer.*`, `src/I2C.*`, `src/CdServo.*`, parts of `src/Bus.*`)

These files were written for this project, but their register layouts and behaviour (and the
HyperScan controller protocol, reset values and memory map used by `System::bootBios`) were taken
from reading MAME's `src/devices/machine/spg290_timer.cpp`, `spg290_i2c.cpp`, `spg290_cdservo.cpp`,
`src/mame/tvgames/spg29x.cpp`, `hyperscan_ctrl.cpp` and `src/devices/cpu/score/score.cpp`
(`license:BSD-3-Clause`, `copyright-holders:Sandro Ronco`). No MAME source file is copied into the
repository. The BSD-3-Clause license text below applies to that derived behaviour.

### BSD-3-Clause license text

Copyright (c) Sandro Ronco and the MAME contributors

Redistribution and use in source and binary forms, with or without modification, are permitted
provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions
   and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright notice, this list of
   conditions and the following disclaimer in the documentation and/or other materials provided
   with the distribution.
3. Neither the name of the copyright holder nor the names of its contributors may be used to
   endorse or promote products derived from this software without specific prior written
   permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
