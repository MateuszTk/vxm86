#pragma once

class Registers {
public:
	enum class Reg : uint8_t {
		// w: 1, bit16: 1, reg: 3
		EAX = 0b10000,
		ECX = 0b10001,
		EDX = 0b10010,
		EBX = 0b10011,
		ESP = 0b10100,
		EBP = 0b10101,
		ESI = 0b10110,
		EDI = 0b10111,

		AX = 0b11000,
		CX = 0b11001,
		DX = 0b11010,
		BX = 0b11011,
		SP = 0b11100,
		BP = 0b11101,
		SI = 0b11110,
		DI = 0b11111,

		AL = 0b00000,
		CL = 0b00001,
		DL = 0b00010,
		BL = 0b00011,
		AH = 0b00100,
		CH = 0b00101,
		DH = 0b00110,
		BH = 0b00111,

		EIP = 0b100000,
		EFLAGS = 0b100001
	};

	enum class Flag : uint32_t {
		CF = 0b0000'0000'0001,
		PF = 0b0000'0000'0100,
		AF = 0b0000'0001'0000,
		ZF = 0b0000'0100'0000,
		SF = 0b0000'1000'0000,
		OF = 0b1000'0000'0000
	};

	Registers() {
		this->reset();
	}

	void setFlag(Flag flag, bool value) {
		uint32_t flags = get(Reg::EFLAGS);
		if (value) {
			flags |= (uint32_t)flag;
		}
		else {
			flags &= ~(uint32_t)flag;
		}
		set(Reg::EFLAGS, flags);
	}

	bool getFlag(Flag flag) {
		uint32_t flags = get(Reg::EFLAGS);
		return (flags & (uint32_t)flag) > 0;
	}

	void set(Reg reg, bool w, bool bit16, uint32_t value) {
		unsigned int index_i = (uint8_t)reg & 0b111;
		if (w) {
			if (bit16) {
				this->registers[index_i] = (this->registers[index_i] & 0xFFFF0000) | (value & 0x0000FFFF);
			}
			else {
				this->registers[index_i] = value;
			}
		}
		else {
			if ((index_i & 0b100) > 0) {
				// AH, CH, DH, BH
				this->registers[index_i - 4] = (this->registers[index_i - 4] & 0xFFFF00FF) | ((value & 0x000000FF) << 8);
			}
			else {
				// AL, CL, DL, BL
				this->registers[index_i] = (this->registers[index_i] & 0xFFFFFF00) | (value & 0x000000FF);
			}
		}
	}

	void set(Reg reg, uint32_t value) {
		unsigned int index_i = (uint8_t)reg;
		if (index_i < 0b100000) {
			this->set(reg, (index_i & 0b10000) > 0, (index_i & 0b01000) > 0, value);
		}
		else {
			this->registers[(index_i & 0b111) + 8] = value;
		}
	}

	uint32_t get(Reg reg, bool w, bool bit16) {
		unsigned int index_i = (uint8_t)reg & 0b111;
		if (w) {
			if (bit16) {
				return this->registers[index_i] & 0x0000FFFF;
			}
			else {
				return this->registers[index_i];
			}
		}
		else {
			if ((index_i & 0b100) > 0) {
				// AH, CH, DH, BH
				return (this->registers[index_i - 4] & 0x0000FF00) >> 8;
			}
			else {
				// AL, CL, DL, BL
				return this->registers[index_i] & 0x000000FF;
			}
		}
	}

	uint32_t get(Reg reg) {
		unsigned int index_i = (uint8_t)reg;
		if (index_i < 0b100000) {
			return this->get(reg, (index_i & 0b10000) > 0, (index_i & 0b01000) > 0);
		}
		else {
			return this->registers[(index_i & 0b111) + 8];
		}
	}

	void print() {
		std::cout << "EAX      " << "ECX      " << "EDX      " << "EBX      " << "ESP      " << "EBP      " << "ESI      " << "EDI      " << "EIP      " << "EFLAGS   " << std::endl;
		std::cout << std::fixed << std::hex << std::setfill('0');
		for (size_t i = 0; i < regCount; i++) {
			std::cout << std::setw(8) << (int)this->registers[i] << " ";
		}
		std::cout << std::endl;
	}

	void reset() {
		memset(this->registers, 0, sizeof(this->registers));
	}

private:
	const static size_t regCount = 8 + 2;
	uint32_t registers[regCount];
};
