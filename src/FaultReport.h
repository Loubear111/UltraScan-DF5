#pragma once
#include <string>

#include "spg290.h"

// Human-readable fault diagnostics for a CPU: the recorded unimplemented/hang
// events and the instruction trace, disassembled with the step-A S+core7
// disassembler (tools/sdis).

// Name of an ECR exception code ("RI", "P_EL", "interrupt", ...), or "unknown".
const char* exceptionName(uint32_t code);

// One line per recorded event, oldest first: PC, raw word, disassembly, what
// was not handled. Hang lines also give the last exception cause (ECR).
std::string formatFaultEvents(const spg290& cpu);

// One line per traced instruction, oldest first: PC, raw word, disassembly.
std::string formatTrace(const spg290& cpu);
