#pragma once

#include "Memory.hpp"
#include "Registers.hpp"
#include <array>
#include <functional>
#include <limits>

class CPU {
public:
	CPU(Memory* memory);

	void run();

	Memory* getMemory();
	Registers& getRegisters();
	void setIP(uint32_t entry);
	void print();
	void setDebug(bool debug);

private:
	Memory* memory;
	Registers registers;
	std::array<std::function<bool(uint8_t opcode, bool sizePrefix)>, 0b11'1111+1> instructions;

	bool debug = false;

	uint32_t readImmediate(bool w, bool bit16);
	uint32_t getEffectiveAddress(uint8_t mod, uint8_t rm);
	void memoryWrite(bool w, bool bit16, uint32_t address, uint32_t value);
	uint32_t memoryRead(bool w, bool bit16, uint32_t address);
	uint32_t rmRead(bool w, bool bit16, uint8_t mod, uint8_t rm);
	void rmWrite(bool w, bool bit16, uint8_t mod, uint8_t rm, uint32_t value);
	void initInstructions();
};
