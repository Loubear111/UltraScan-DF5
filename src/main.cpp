#include <iostream>
#include "Bus.h"

int main()
{
	using namespace std;

	Bus HyperScan;

	// Here we're assigning values to arbitrary locations to test instruction to register funcitionality
	HyperScan.write32(10, 0xFFFF45E6); // A register
	HyperScan.write32(11, 0b0010); // B register
	HyperScan.write32(12, 0xFFFFFFFF); // D register

	// if you decode this instruction you'll see it uses the registers set above
	// we're just assuming PC starts at 0 and the first instruction is at 0 in memory
	HyperScan.write32(0, 0x818A2C20); // ANDX D, A, B
	HyperScan.write32(0, 0x85937FFE); // ANDIX D, 0xFFFF
	HyperScan.write32(0, 0xB18A469C); // ANDRIX D, A, 0x234E

	cout << "***HyperScan CPU Test***" << endl;

	cout << "MEMORY DUMP (PRE-EXECUTION):" << endl;

	for (int i = 0; i < 14; i++)
	{
		cout << hex << i;
		cout << ": ";
		cout << hex << HyperScan.read32((uint32_t)i);
		if (i == 10)
		{
			cout << "	(A reg)";
		}
		else if (i == 11)
		{
			cout << "	(B reg)";
		}
		else if (i == 12)
		{
			cout << "	(D reg)";
		}
		cout << endl;
	}

	cout << endl << "PC: " << hex << HyperScan.cpu.pc << endl;

	//go through one clock cycle
	HyperScan.cpu.clock();

	cout << "MEMORY DUMP (POST-EXECUTION):" << endl;

	for (int i = 0; i < 14; i++)
	{
		cout << hex << i;
		cout << ": ";
		cout << hex << HyperScan.read32((uint32_t)i);
		if (i == 10)
		{
			cout << "	(A reg)";
		}
		else if (i == 11)
		{
			cout << "	(B reg)";
		}
		else if (i == 12)
		{
			cout << "	(D reg)";
		}
		cout << endl;
	}

	cout << endl << "PC: " << hex << HyperScan.cpu.pc << endl;

	return 0;
}