#include "CPU.hpp"


CPU::CPU(Memory* memory) :
	memory(memory),
	registers() {
	registers.set(Registers::Reg::EIP, 0x00000000);

	initInstructions();
}

void CPU::run() {
	while (true) {

		uint32_t eip = this->registers.get(Registers::Reg::EIP);

		if (debug) {
			this->print();

			// wait for keypress
			char inst;
			while (true) {
				std::cin >> inst;
				std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
				if (inst == 's') {
					break;
				}
				else if (inst == 'c') {
					this->setDebug(false);
					break;
				}
				else {
					std::cout << "Unknown debug command" << std::endl;
				}
			}
		}

		uint8_t opcode = readImmediate(false, false);

		bool sizePrefix = (opcode == 0x66);
		if (sizePrefix) {
			// if size prefix, read next opcode
			opcode = readImmediate(false, false);
		}

		// execute instruction
		bool result = this->instructions[opcode >> 2](opcode, sizePrefix);
		if (!result) {
			break;
		}
	}
}

Memory* CPU::getMemory() {
	return this->memory;
}

Registers& CPU::getRegisters() {
	return this->registers;
}

void CPU::setIP(uint32_t entry) {
	this->registers.set(Registers::Reg::EIP, entry);
}

void CPU::print() {
	std::cout << "Registers:" << std::endl;
	this->registers.print();

	std::cout << "Memory:" << std::endl;
	uint32_t eip = this->registers.get(Registers::Reg::EIP);
	if (this->memory != nullptr) this->memory->print(16, eip);
}

void CPU::setDebug(bool debug) {
	this->debug = debug;

	if (debug) {
		std::cout << "***********************************" << std::endl;
		std::cout << "Debug mode enabled" << std::endl;
		std::cout << "Commands:" << std::endl;
		std::cout << "s - step over" << std::endl;
		std::cout << "c - continue / disable debug mode" << std::endl;
		std::cout << "***********************************" << std::endl;
	}
	else {
		std::cout << "***********************************" << std::endl;
		std::cout << "Debug mode disabled" << std::endl;
		std::cout << "***********************************" << std::endl;
	}
}

uint32_t CPU::readImmediate(bool w, bool bit16) {
	uint32_t eip = this->registers.get(Registers::Reg::EIP);
	uint32_t value = this->memory->read<uint32_t>(eip);
	int size = (w ? (bit16 ? 2 : 4) : 1);
	uint32_t mask = (0xff'ff'ff'ffull << (size * 8));
	mask = ~mask;
	this->registers.set(Registers::Reg::EIP, eip + size);
	return (value & mask);
}

uint32_t CPU::getEffectiveAddress(uint8_t mod, uint8_t rm) {
	uint32_t address = 0;
	if (rm == 0b100) {
		// SIB
		uint8_t sib = readImmediate(false, false);

		uint8_t base = (sib & 0b0000'0111);
		rm = base;

		uint8_t index = (sib & 0b0011'1000) >> 3;
		uint8_t scale = (sib & 0b1100'0000) >> 6;


		if (index != 0b100) {
			uint32_t indexValue = this->registers.get((Registers::Reg)index, true, false);
			address += indexValue << scale;
		}
	}

	if (mod == 0b00) {
		// indirect
		if (rm == 0b101) {
			// disp32
			return address + readImmediate(true, false);
		}
		else {
			// register
			return address + this->registers.get((Registers::Reg)rm, true, false);
		}
	}
	else if (mod == 0b01) {
		// reg + disp8
		// TODO: segments
		address += this->registers.get((Registers::Reg)rm, true, false);
		int8_t disp8 = readImmediate(false, false);
		return address + disp8;
	}
	else if (mod == 0b10) {
		// reg + disp32
		// TODO: segments
		address += this->registers.get((Registers::Reg)rm, true, false);
		int32_t disp32 = readImmediate(true, false);
		return address + disp32;
	}
	else {
		std::cout << "Not implemented: mod = " << (int)mod << std::endl;
	}

	return 0;
}

void CPU::memoryWrite(bool w, bool bit16, uint32_t address, uint32_t value) {
	if (w) {
		if (bit16) {
			this->memory->write<uint16_t>(address, value);
		}
		else {
			this->memory->write<uint32_t>(address, value);
		}
	}
	else {
		this->memory->write<uint8_t>(address, value);
	}
}

uint32_t CPU::memoryRead(bool w, bool bit16, uint32_t address) {
	if (w) {
		if (bit16) {
			return this->memory->read<uint16_t>(address);
		}
		else {
			return this->memory->read<uint32_t>(address);
		}
	}
	else {
		return this->memory->read<uint8_t>(address);
	}
}

uint32_t CPU::rmRead(bool w, bool bit16, uint8_t mod, uint8_t rm) {
	if (mod == 0b11) {
		// r/m is register
		Registers::Reg reg = (Registers::Reg)rm;
		return this->registers.get(reg, w, bit16);
	}
	else {
		// r/m is memory
		uint32_t address = getEffectiveAddress(mod, rm);
		return memoryRead(w, bit16, address);
	}
}

void CPU::rmWrite(bool w, bool bit16, uint8_t mod, uint8_t rm, uint32_t value) {
	if (mod == 0b11) {
		// r/m is register
		Registers::Reg reg = (Registers::Reg)rm;
		this->registers.set(reg, w, bit16, value);
	}
	else {
		// r/m is memory
		uint32_t address = getEffectiveAddress(mod, rm);
		memoryWrite(w, bit16, address, value);
	}
}
