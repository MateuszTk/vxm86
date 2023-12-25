#include <iostream>
#include <iomanip>
#include <fstream>
#include <string>
#include <vector>

#include "Memory.hpp"
#include "Registers.hpp"
#include "CPU.hpp"
#include "ELFLoader.hpp"

void codeArray() {
	const uint8_t code[] = {
		0x66, 0xBB, 0x08, 0x00,			// mov bx, 8
		0xB1, 0x04,						// mov cl, 4
		0xB6, 0x02,						// mov dh, 2

		0xB8, 0x40, 0x00, 0x00, 0x00,	// mov eax, 64
		0xC6, 0x00, 0x58,				// mov byte ptr [eax], 0x58
		0xC6, 0x40, 0x01, 0x59,			// mov byte ptr [eax + 1], 0x59

		0x66, 0xBF, 0x40, 0x00,			// mov di, 64
		0x8B, 0xF7,						// mov esi, edi
		0xC6, 0x46, 0xFF, 0x57,			// mov byte ptr [esi - 1], 0x57

		0x90,							// nop
		0xF4							// hlt
	};

	Memory mem(128);
	mem.write(0, code, sizeof(code));

	CPU cpu(&mem);
	cpu.setDebug(true);
	cpu.run();
	cpu.print();
}

void elf() {
	Memory mem(0x0f'ff'ff'ff);

	ELFLoader loader("./elf/elf_test");
	uint32_t entry = loader.load(mem);
	mem.setModifiedRangeFrom(entry);

	CPU cpu(&mem);
	cpu.setDebug(true);
	cpu.setIP(entry);
	cpu.getRegisters().set(Registers::Reg::ESP, 0x0f'ff'ff'00);

	cpu.run();
	//cpu.print();
}

int main() {
	try {
		//codeArray();
		elf();
	}
	catch (const std::exception& e) {
		std::cout << e.what() << std::endl;
	}

	return 0;
}

