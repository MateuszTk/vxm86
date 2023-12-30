#pragma once

#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <iostream>
#include <iomanip>

class Memory {
public:
	Memory(size_t size) :
		size(size),
		data(new uint8_t[size]) {
	}

	template<typename T>
	void write(size_t address, T value) {
		if (address + sizeof(T) > this->size) {
			throw std::runtime_error("Out of bounds");
		}

		*((T*)(this->data + address)) = value;

		if (address < modifiedFrom) {
			modifiedFrom = address;
		}
		if (address + sizeof(T) > modifiedTo) {
			modifiedTo = address + sizeof(T);
		}
	}

	void write(size_t address, const uint8_t* data, size_t size) {
		if (address + size > this->size) {
			throw std::runtime_error("Out of bounds");
		}

		memcpy(this->data + address, data, size);

		if (address < modifiedFrom) {
			modifiedFrom = address;
		}
		if (address + size > modifiedTo) {
			modifiedTo = address + size;
		}
	}

	template<typename T>
	T read(size_t address) {
		if (address + sizeof(T) > this->size) {
			throw std::runtime_error("Out of bounds");
		}

		return *((T*)(this->data + address));
	}

	void read(size_t address, uint8_t* data, size_t size) {
		if (address + size > this->size) {
			throw std::runtime_error("Out of bounds");
		}

		memcpy(data, this->data + address, size);
	}

	void clear() {
		memset(this->data, 0, this->size);
	}

	void clear(size_t from, size_t _size) {
		memset(this->data + from, 0, _size);
	}

	void print(size_t rowSize = 16, uint32_t eip = -1) {
		std::cout << std::fixed << std::hex << std::setfill('0');
		size_t modifiedToCeil = this->modifiedTo + rowSize - (this->modifiedTo % rowSize);
		int emptyLine = 0;

		for (size_t i = (modifiedFrom / rowSize) * rowSize; i < modifiedToCeil; i += rowSize) {
			if (emptyLine == 0) {
				std::cout << std::setw(8) << i << ": ";
				std::cout << std::setw(2);
			}

			int firstByte = this->data[i];
			bool wasEmpty = true;
			std::string ascii = "";

			for (size_t j = 0; j < rowSize && i + j < modifiedToCeil; j++) {
				if (i + j == eip && emptyLine == 0) {
					std::cout << "\033[1;31m";
					ascii += "\033[1;31m";
				}

				int cbyte = this->data[j + i];
				if (cbyte != firstByte) {
					wasEmpty = false;
				}

				if (emptyLine == 0) {
					std::cout << std::setw(2) << cbyte << " ";
					ascii.push_back((cbyte < ' ' || cbyte > '~') ? '.' : cbyte);
				}

				if (i + j == eip && emptyLine == 0) {
					std::cout << "\033[0m";
					ascii += "\033[0m";
				}
			}
		
			
			std::cout << ' ' << ascii;
			ascii.clear();

			if (emptyLine == 0) {
				std::cout << std::endl;
			}		

			if (wasEmpty) {
				emptyLine++;
			}

			if (emptyLine > 0) {
				if (emptyLine == 1) {
					std::cout << "....\n";
				}
				if (!wasEmpty) {
					i -= rowSize;
					emptyLine = 0;
				}
			}
		}
	}

	void setModifiedRangeFrom(size_t modifiedFrom) {
		this->modifiedFrom = modifiedFrom;
	}

	void setModifiedRangeTo(size_t modifiedTo) {
		this->modifiedTo = modifiedTo;
	}

	~Memory() {
		delete[] data;
	}
private:
	size_t size;
	uint8_t* data;
	size_t modifiedFrom = 0xff'ff'ff'ff;
	size_t modifiedTo = 0;
};
