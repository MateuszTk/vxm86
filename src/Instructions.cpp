#include "CPU.hpp"
#include <string>

void CPU::initInstructions() {
	auto invalidInstruction = [&](uint8_t opcode, bool sizePrefix) -> bool {
		std::cout << "Unknown opcode: " << (int)opcode << std::endl;
		return false;
	};

	// set all instructions to invalid
	for (auto& instr : instructions) {
		instr = invalidInstruction;
	}

	instructions[0x90 >> 2] = [&](uint8_t opcode, bool sizePrefix) -> bool {
		// NOP					
		return true;
	};


	instructions[0xF4 >> 2] = [&](uint8_t opcode, bool sizePrefix) -> bool {
		// HLT
		return false;
	};

	instructions[0b1011'0000 >> 2] = instructions[0b1011'1000 >> 2] = instructions[0b1011'0100 >> 2] = instructions[0b1011'1100 >> 2] =
		[&](uint8_t opcode, bool sizePrefix) -> bool {
		// mov r, imm
		// [1011 w reg] [imm]
		bool w = (opcode & 0b0000'1000) > 0;
		Registers::Reg reg = (Registers::Reg)(opcode & 0b0000'0111);
		uint32_t value = readImmediate(w, sizePrefix);
		this->registers.set(reg, w, sizePrefix, value);
		return true;
	};

	instructions[0b1100'0100 >> 2] = [&](uint8_t opcode, bool sizePrefix) -> bool {
		// mov r/m, imm
		// [1100 011 w] [mod 000 r/m] [imm]
		bool w = (opcode & 0b0000'1000) > 0;

		uint8_t modrm = readImmediate(false, false);

		uint8_t mod = (modrm & 0b1100'0000) >> 6;
		uint8_t rm = (modrm & 0b0000'0111);

		if (mod == 0b11) {
			// r/m is register
			Registers::Reg reg = (Registers::Reg)rm;
			uint32_t value = readImmediate(w, sizePrefix);
			this->registers.set(reg, w, sizePrefix, value);
		}
		else {
			// r/m is memory
			uint32_t address = getEffectiveAddress(mod, rm);
			uint32_t value = readImmediate(w, sizePrefix);
			memoryWrite(w, sizePrefix, address, value);
		}
		return true;
	};

	instructions[0b1000'1000 >> 2] = [&](uint8_t opcode, bool sizePrefix) -> bool {
		// [1000 10 d w] [mod reg r/m]
		bool w = (opcode & 0b0000'0001) > 0;
		bool d = (opcode & 0b0000'0010) > 0;

		uint32_t modregrm = readImmediate(false, false);

		uint8_t mod = (modregrm & 0b1100'0000) >> 6;
		uint8_t reg = (modregrm & 0b0011'1000) >> 3;
		uint8_t rm = (modregrm & 0b0000'0111);

		if (d) {
			//mov r, r/m
			uint32_t value = rmRead(w, sizePrefix, mod, rm);
			Registers::Reg regEnum = (Registers::Reg)reg;
			this->registers.set(regEnum, w, sizePrefix, value);
		}
		else {
			//mov r/m, r
			uint32_t value = this->registers.get((Registers::Reg)reg, w, sizePrefix);
			rmWrite(w, sizePrefix, mod, rm, value);
		}

		return true;
	};

	instructions[0b0000'0000 >> 2] = [&](uint8_t opcode, bool sizePrefix) -> bool {
		// add r/m, r
		// [0000 00 d w] [mod reg r/m]
		bool w = (opcode & 0b0000'0001) > 0;
		bool d = (opcode & 0b0000'0010) > 0;

		uint32_t modregrm = readImmediate(false, false);

		uint8_t mod = (modregrm & 0b1100'0000) >> 6;
		uint8_t reg = (modregrm & 0b0011'1000) >> 3;
		uint8_t rm = (modregrm & 0b0000'0111);

		uint32_t value1 = this->registers.get((Registers::Reg)reg, w, sizePrefix);

		// rmRead will increment eip if needed to read immediate
		uint32_t eipBackup = this->registers.get(Registers::Reg::EIP);
		uint32_t value2 = rmRead(w, sizePrefix, mod, rm);

		uint32_t result = value1 + value2;
		if (d) {
			// add r, r/m
			this->registers.set((Registers::Reg)reg, w, sizePrefix, result);
		}
		else {
			// add r/m, r
			// restore eip to before rmRead, since we need to read the immediate again
			this->registers.set(Registers::Reg::EIP, eipBackup);
			rmWrite(w, sizePrefix, mod, rm, result);
		}

		return true;
	};

	instructions[0b1000'0000 >> 2] = [&](uint8_t opcode, bool sizePrefix) -> bool {
		// add r/m, imm
		// [1000 00 s w] [mod 000 r/m] [imm]
		// 
		// sub r/m, imm
		// [1000 00 s w] [mod 101 r/m] [imm]
		// 
		// s = 0 -> imm8/16/32 
		// s = 1 -> imm8 sign extended to imm16/32

		bool w = (opcode & 0b0000'0001) > 0;
		bool s = (opcode & 0b0000'0010) > 0;

		uint32_t modregrm = readImmediate(false, false);

		uint8_t mod = (modregrm & 0b1100'0000) >> 6;
		uint8_t op = (modregrm & 0b0011'1000) >> 3;
		uint8_t rm = (modregrm & 0b0000'0111);

		uint32_t eipBackup = this->registers.get(Registers::Reg::EIP);
		uint32_t value1 = rmRead(w, sizePrefix, mod, rm);
		uint32_t value2 = readImmediate(w && !s, sizePrefix);

		if (s) {
			// sign extend imm8 to imm16/32
			value2 = (int32_t)(int8_t)value2;
		}

		this->registers.setFlag(Registers::Flag::ZF, value1 == value2);

		uint32_t result = 0;
		if (op == 0b000) {
			// add
			result = value1 + value2;
			this->registers.setFlag(Registers::Flag::CF, result < value1);
		}
		else if (op == 0b101 || op == 0b111) {
			// sub
			result = value1 - value2;
			this->registers.setFlag(Registers::Flag::CF, value2 > value1);
		}
		else {
			// not implemented
			std::cout << "Not implemented variant of 0b1000'0000" << std::endl;
		}

		if (op == 0b000 || op == 0b101) {
			// restore eip to before rmRead, since we need to read the immediate again, then set it to the instruction end
			uint32_t eipBackup2 = this->registers.get(Registers::Reg::EIP);
			this->registers.set(Registers::Reg::EIP, eipBackup);
			rmWrite(w, sizePrefix, mod, rm, result);
			this->registers.set(Registers::Reg::EIP, eipBackup2);
		}

		return true;
	};

	instructions[0b1110'0000 >> 2] = [&](uint8_t opcode, bool sizePrefix) -> bool {
		// loop
		// [1110 00 type] [imm8]
		uint8_t type = (opcode & 0b0000'0011);
		int8_t displacement = readImmediate(false, false);

		if (type == 0b10) {
			// loop
			uint32_t ecx = this->registers.get(Registers::Reg::ECX);
			ecx -= 1;
			this->registers.set(Registers::Reg::ECX, ecx);
			if (ecx != 0) {
				this->registers.set(Registers::Reg::EIP, this->registers.get(Registers::Reg::EIP) + displacement);
			}
		}
		else {
			std::cout << "Unknown loop type: " << (int)type << std::endl;
			return false;
		}
		return true;
	};

	instructions[0b01110100 >> 2] = [&](uint8_t opcode, bool sizePrefix) -> bool {
		// jz/jnz
		// [0111 010 n] [imm8]	
		bool n = (opcode & 1) > 0;
		int8_t displacement = readImmediate(false, false);
		if (this->registers.getFlag(Registers::Flag::ZF) != n) {
			this->registers.set(Registers::Reg::EIP, this->registers.get(Registers::Reg::EIP) + (int32_t)displacement);
		}
		return true;
	};

	instructions[0b1100'1100 >> 2] = [&](uint8_t opcode, bool sizePrefix) -> bool {
		if (opcode == 0b1100'1101) {
			// int
			// [1100 1101] [imm8]

			uint8_t intId = readImmediate(false, false);

			if (intId == 0x80) {
				// syscall
				uint32_t eax = this->registers.get(Registers::Reg::EAX);
				uint32_t ebx = this->registers.get(Registers::Reg::EBX);
				uint32_t ecx = this->registers.get(Registers::Reg::ECX);
				uint32_t edx = this->registers.get(Registers::Reg::EDX);
				uint32_t esi = this->registers.get(Registers::Reg::ESI);
				uint32_t edi = this->registers.get(Registers::Reg::EDI);

				// green text
				std::cout << "\033[1;32m";

				switch (eax) {
					case 1:
					{
						// sys_exit
						// ebx = exit code
						std::cout << "Program exited with code " << ebx << std::endl;
						std::cout << "\033[0m";
						return false;
					}

					case 3:
					{
						// sys_read
						// ebx = file descriptor (0 = stdin)
						// ecx = buffer
						// edx = size

						char* tmpBuffer = new char[edx];
						std::cin.getline(tmpBuffer, edx);
						int len = strlen(tmpBuffer);
						if (len + 1 < edx) {
							tmpBuffer[len] = '\n';
							tmpBuffer[len + 1] = '\0';
						}
						this->memory->write(ecx, (uint8_t*)tmpBuffer, edx);
						return true;
					}

					case 4:
					{
						// sys_write
						// ebx = file descriptor (1 = stdout)
						// ecx = buffer
						// edx = size

						char* tmpBuffer = new char[edx + 1];
						tmpBuffer[edx] = '\0';
						this->memory->read(ecx, (uint8_t*)tmpBuffer, edx);

						std::cout << tmpBuffer;
						return true;
					}

					default:
					{
						std::cout << "Unknown syscall: " << eax << std::endl;
						std::cout << "\033[0m";
						return false;
					}
				}

				std::cout << "\033[0m";
			}
			else {
				std::cout << "Unknown interrupt: " << (int)intId << std::endl;
				return false;
			}
		}
		else {
			std::cout << "Unknown int opcode: " << std::hex << (int)opcode << std::endl;
			return false;
		}
		return true;
	};

	instructions[0b0101'0000 >> 2] = instructions[0b0101'0100 >> 2]  = [&](uint8_t opcode, bool sizePrefix) -> bool {
		// push r
		// [0101 0 reg]

		uint8_t reg = (opcode & 0b0000'0111);
		uint32_t value = this->registers.get((Registers::Reg)reg, true, sizePrefix);
		uint32_t esp = this->registers.get(Registers::Reg::ESP);

		if (sizePrefix) {
			esp -= 2;
			this->memory->write<uint16_t>(esp, value);
		}
		else {
			esp -= 4;
			this->memory->write<uint32_t>(esp, value);
		}

		this->registers.set(Registers::Reg::ESP, esp);
		return true;
	};

	instructions[0b0110'1000 >> 2] = [&](uint8_t opcode, bool sizePrefix) -> bool {
		// push imm
		// [0110 10 s 0] [imm]

		uint32_t value1 = 0xff;
		value1 = (int32_t)(int8_t)value1;

		bool s = (opcode & 0b0000'0010);
		uint32_t value = readImmediate(!s, sizePrefix);
		uint32_t esp = this->registers.get(Registers::Reg::ESP);

		if (s) {
			// sign extend imm8 to imm16/32
			value = (int32_t)(int8_t)value;
		}

		if (sizePrefix) {
			esp -= 2;
			this->memory->write<uint16_t>(esp, value);
		}
		else {
			esp -= 4;
			this->memory->write<uint32_t>(esp, value);
		}

		this->registers.set(Registers::Reg::ESP, esp);
		return true;
	};

	instructions[0b0101'1000 >> 2] = instructions[0b0101'1100 >> 2] = [&](uint8_t opcode, bool sizePrefix) -> bool {
		// pop r
		// [0101 1 reg]

		uint8_t reg = (opcode & 0b0000'0111);
		uint32_t esp = this->registers.get(Registers::Reg::ESP);

		uint32_t value;
		if (sizePrefix) {
			value = this->memory->read<uint16_t>(esp);
			esp += 2;
		}
		else {
			value = this->memory->read<uint32_t>(esp);
			esp += 4;
		}

		this->registers.set((Registers::Reg)reg, true, sizePrefix, value);
		this->registers.set(Registers::Reg::ESP, esp);

		return true;
	};

	instructions[0b1110'1000 >> 2] = [&](uint8_t opcode, bool sizePrefix) -> bool {
		// call rel16/32
		// [1110 1000] [rel16/32]

		// jmp rel16/32
		// [1110 1001] [rel16/32]

		// jmp rel8
		// [1110 1011] [rel8]

		uint8_t op = (opcode & 0b0000'0011);

		bool s = (opcode & 0b0000'0010) > 0;

		uint32_t rel = readImmediate(!s, sizePrefix);
		uint32_t eip = this->registers.get(Registers::Reg::EIP);
		uint32_t esp = this->registers.get(Registers::Reg::ESP);

		if (s) {
			// sign extend imm8 to imm16/32
			rel = (int32_t)(int8_t)rel;
		}

		if (sizePrefix) {
			esp -= 2;
			this->memory->write<uint16_t>(esp, eip);
			rel = (int32_t)(int16_t)rel;
			eip += rel;
			eip &= 0xffff;
		}
		else {
			esp -= 4;
			this->memory->write<uint32_t>(esp, eip);
			eip += rel;
		}

		if (op == 0b00) {
			this->registers.set(Registers::Reg::ESP, esp);
		}

		this->registers.set(Registers::Reg::EIP, eip);
		return true;
	};

	instructions[0b0110'0000 >> 2] = [&](uint8_t opcode, bool sizePrefix) -> bool {
		uint32_t esp = this->registers.get(Registers::Reg::ESP);
		if ((opcode & 1) == 1) {
			// popa(d)
			// [0110 0001]

			uint32_t value;
			uint32_t increment = (sizePrefix) ? 2 : 4;

			value = this->memory->read<uint32_t>(esp);
			esp += increment;
			this->registers.set(Registers::Reg::EDI, true, sizePrefix, value);

			value = this->memory->read<uint32_t>(esp);
			esp += increment;
			this->registers.set(Registers::Reg::ESI, value);

			value = this->memory->read<uint32_t>(esp);
			esp += increment;
			this->registers.set(Registers::Reg::EBP, value);

			esp += increment; // skip ESP

			value = this->memory->read<uint32_t>(esp);
			esp += increment;
			this->registers.set(Registers::Reg::EBX, value);

			value = this->memory->read<uint32_t>(esp);
			esp += increment;
			this->registers.set(Registers::Reg::EDX, value);

			value = this->memory->read<uint32_t>(esp);
			esp += increment;
			this->registers.set(Registers::Reg::ECX, value);

			value = this->memory->read<uint32_t>(esp);
			esp += increment;
			this->registers.set(Registers::Reg::EAX, value);
		}
		else {
			// pusha(d)
			// [0110 0000]

			auto pushReg = [&](Registers::Reg reg) {
				uint32_t value = this->registers.get(reg);
				esp -= (sizePrefix) ? 2 : 4;
				this->memory->write<uint32_t>(esp, value);
			};

			pushReg(Registers::Reg::EAX);
			pushReg(Registers::Reg::ECX);
			pushReg(Registers::Reg::EDX);
			pushReg(Registers::Reg::EBX);
			pushReg(Registers::Reg::ESP);
			pushReg(Registers::Reg::EBP);
			pushReg(Registers::Reg::ESI);
			pushReg(Registers::Reg::EDI);
		}
		this->registers.set(Registers::Reg::ESP, esp);
		return true;
	};

	instructions[0b1100'0000 >> 2] = [&](uint8_t opcode, bool sizePrefix) -> bool {
		// ret (near)
		// [1100 0011]

		if ((opcode & 0b11) != 0b11) {
			std::cout << "Unknown opcode: " << (int)opcode << std::endl;
			return false;
		}

		uint32_t esp = this->registers.get(Registers::Reg::ESP);
		uint32_t eip;

		if (sizePrefix) {
			eip = this->memory->read<uint16_t>(esp);
			esp += 2;
		}
		else {
			eip = this->memory->read<uint32_t>(esp);
			esp += 4;
		}

		this->registers.set(Registers::Reg::ESP, esp);
		this->registers.set(Registers::Reg::EIP, eip);
		return true;
	};

	instructions[0b0100'0000 >> 2] = instructions[0b0100'0100 >> 2] = instructions[0b0100'1000 >> 2] = instructions[0b0100'1100 >> 2] =
		[&](uint8_t opcode, bool sizePrefix) -> bool {
		// inc reg16/32
		// [0100 0 reg]

		// dec reg16/32
		// [0100 1 reg]

		uint8_t reg = opcode & 0b111;
		bool inc = (opcode & 0b100) == 0;

		uint32_t value = this->registers.get((Registers::Reg)reg, true, sizePrefix);
		if (inc) value++;
		else value--;

		this->registers.set((Registers::Reg)reg, true, sizePrefix, value);
		return true;
	};
}
