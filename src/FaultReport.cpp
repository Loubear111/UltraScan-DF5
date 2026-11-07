#include "FaultReport.h"

#include <cstdio>

#include "score_disasm.h"

const char* exceptionName(uint32_t code)
{
	switch (code)
	{
	case 0:  return "none";
	case 1:  return "NMI";
	case 2:  return "AdEL (instruction)";
	case 5:  return "IBE";
	case 6:  return "P_EL";
	case 7:  return "syscall";
	case 8:  return "CCU";
	case 9:  return "RI";
	case 10: return "trap";
	case 11: return "AdEL (data)";
	case 12: return "AdES (data)";
	case 16: return "CEE";
	case 17: return "CPE";
	case 18: return "DBE";
	case 20: return "interrupt";
	default: return "unknown";
	}
}

static std::string disasm(uint32_t pc, uint32_t word)
{
	int size = 0;
	return score7Disasm(pc, word, size);
}

std::string formatFaultEvents(const spg290& cpu)
{
	std::string out;
	char line[256];
	for (const spg290::FaultEvent& e : cpu.faultEvents())
	{
		const uint32_t code = e.cause & spg290::ECR_CODE_MASK;
		if (e.kind == spg290::FaultEvent::Hang)
			std::snprintf(line, sizeof(line),
			              "Hang at PC 0x%08X: %s; last exception cause %s (%u)\n",
			              e.pc, e.what.c_str(), exceptionName(code), code);
		else
			std::snprintf(line, sizeof(line),
			              "Unimplemented at PC 0x%08X: word 0x%08X  %s  [%s]\n",
			              e.pc, e.word, disasm(e.pc, e.word).c_str(), e.what.c_str());
		out += line;
	}
	return out;
}

std::string formatTrace(const spg290& cpu)
{
	std::string out;
	char line[160];
	for (const spg290::TraceEntry& t : cpu.traceEntries())
	{
		if (t.size == 4)
			std::snprintf(line, sizeof(line), "%08X:  %08X  %s\n", t.pc, t.word, disasm(t.pc, t.word).c_str());
		else
			std::snprintf(line, sizeof(line), "%08X:  %04X      %s\n", t.pc, t.word & 0xFFFFu, disasm(t.pc, t.word).c_str());
		out += line;
	}
	return out;
}
