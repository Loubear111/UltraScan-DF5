/* score.ld — Minimal linker script for HyperScan/SPG290 test ROMs.
 *
 * Places the entire .text section at byte address 0x0000 so that the
 * emulator can load the flat binary directly into bus.ram[] starting at
 * word index 0 (which is where PC starts).
 */
OUTPUT_FORMAT("elf32-bigscore")
OUTPUT_ARCH(score)

SECTIONS
{
    . = 0x00000000;
    .text : { *(.text) }
}
