#pragma once

#include "Memory.hpp"
#include "Registers.hpp"

class CPU {
public:
	CPU(Memory* memory) :
		memory(memory),
		registers() {
		registers.set(Registers::Reg::EIP, 0x00000000);
	}

	void run() {
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

			switch (opcode & 0b1111'1100) {
				case 0x90:
				{
					// NOP					
					break;
				}

				case 0xF4:
				{
					// HLT
					return;
				}

				case 0b1011'0000:
				case 0b1011'1000:
				case 0b1011'0100:
				case 0b1011'1100:
				{
					// mov r, imm
					// [1011 w reg] [imm]
					bool w = (opcode & 0b0000'1000) > 0;
					Registers::Reg reg = (Registers::Reg)(opcode & 0b0000'0111);
					uint32_t value = readImmediate(w, sizePrefix);
					this->registers.set(reg, w, sizePrefix, value);
					break;
				}

				case 0b1100'0100:
				{
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
					break;
				}

				case 0b1000'1000:
				{
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

					break;
				}

				case 0b0000'0000:
				{
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

					break;
				}

				case 0b1000'0000:
				{
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
					break;
				}

				case 0b1110'0000:
				{
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
						return;
					}
					break;
				}

				case 0b01110100:
				{
					// jz/jnz
					// [0111 010 n] [imm8]	
					bool n = (opcode & 1) > 0;
					int8_t displacement = readImmediate(false, false);
					if (this->registers.getFlag(Registers::Flag::ZF) != n) {					
						this->registers.set(Registers::Reg::EIP, this->registers.get(Registers::Reg::EIP) + (int32_t)displacement);
					}
					break;
				}

				case 0b1100'1100:
				{
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
									return;
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
									break;
								}

								case 4:
								{
									// sys_write
									// ebx = file descriptor (1 = stdout)
									// ecx = buffer
									// edx = size

									char* tmpBuffer = new char[edx+1];
									tmpBuffer[edx] = '\0';
									this->memory->read(ecx, (uint8_t*)tmpBuffer, edx);

									std::cout << tmpBuffer;
									break;
								}

								default:
								{
									std::cout << "Unknown syscall: " << eax << std::endl;
									std::cout << "\033[0m";
									return;
								}
							}

							std::cout << "\033[0m";
						}
						else {
							std::cout << "Unknown interrupt: " << (int)intId << std::endl;
							return;
						}
					}
					else {
						std::cout << "Unknown int opcode: " << std::hex << (int)opcode << std::endl;
						return;
					}
					break;
				}

				case 0b0101'0000:
				case 0b0101'0100:
				{
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

					break;
				}

				case 0b0110'1000:
				{
					// push imm
					// [0110 10 s 0] [imm]

					uint32_t value1 = 0xff;
					value1 = (int32_t)(int8_t)value1;

					bool s = (opcode & 0b0000'0010);
					uint32_t value = readImmediate(!s, sizePrefix);
					uint32_t esp = this->registers.get(Registers::Reg::ESP);

					if (s){
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

					break;
				}

				case 0b0101'1000:
				case 0b0101'1100:
				{
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

					break;
				}

				case 0b1110'1000:
				{
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

					break;
				}

				case 0b0110'0000:
				{
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
					break;
				}

				case 0b1100'0000:
				{
					// ret (near)
					// [1100 0011]

					if ((opcode & 0b11) != 0b11) {
						std::cout << "Unknown opcode: " << (int)opcode << std::endl;
						return;
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

					break;
				}

				case 0b0100'0000:
				case 0b0100'0100:
				case 0b0100'1000:
				case 0b0100'1100:
				{
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

					break;
				}

				default:
				{
					std::cout << "Unknown opcode: " << (int)opcode << std::endl;
					return;
				}
			}
		}
	}

	Memory* getMemory() {
		return this->memory;
	}

	Registers& getRegisters() {
		return this->registers;
	}

	void setIP(uint32_t entry) {
		this->registers.set(Registers::Reg::EIP, entry);
	}

	void print() {
		std::cout << "Registers:" << std::endl;
		this->registers.print();

		std::cout << "Memory:" << std::endl;
		uint32_t eip = this->registers.get(Registers::Reg::EIP);
		if (this->memory != nullptr) this->memory->print(16, eip);
	}

	void setDebug(bool debug) {
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

private:
	Memory* memory;
	Registers registers;

	bool debug = false;

	uint32_t readImmediate(bool w, bool bit16) {
		uint32_t eip = this->registers.get(Registers::Reg::EIP);
		uint32_t value = this->memory->read<uint32_t>(eip);
		int size = (w ? (bit16 ? 2 : 4) : 1);
		uint32_t mask = (0xff'ff'ff'ffull << (size * 8));
		mask = ~mask;
		this->registers.set(Registers::Reg::EIP, eip + size);
		return (value & mask);
	}

	uint32_t getEffectiveAddress(uint8_t mod, uint8_t rm) {
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

	void memoryWrite(bool w, bool bit16, uint32_t address, uint32_t value) {
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

	uint32_t memoryRead(bool w, bool bit16, uint32_t address) {
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

	uint32_t rmRead(bool w, bool bit16, uint8_t mod, uint8_t rm) {
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

	void rmWrite(bool w, bool bit16, uint8_t mod, uint8_t rm, uint32_t value) {
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
};
