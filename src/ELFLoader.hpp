#pragma once

#include <string>
#include <fstream>
#include <vector>
#include "Memory.hpp"

class ELFLoader {
public:
	ELFLoader(const std::string& path) {
		std::ifstream file(path, std::ios::binary);
		if (!file.is_open()) {
			throw std::runtime_error("Failed to open file");
		}

		file.seekg(0, std::ios::end);
		size_t size = file.tellg();
		file.seekg(0, std::ios::beg);
		data.resize(size);
		file.read((char*)data.data(), size);

		if (data[0] != 0x7f || data[1] != 'E' || data[2] != 'L' || data[3] != 'F') {
			throw std::runtime_error("Invalid ELF file");
		}
	}

	// Load the ELF file into memory and return the entry point.
	uint32_t load(Memory& memory) {
		uint32_t entry = *(uint32_t*)(data.data() + 0x18);

		uint32_t shoff = *(uint32_t*)(data.data() + 0x20);
		uint16_t shentsize = *(uint16_t*)(data.data() + 0x2E);
		uint16_t shnum = *(uint16_t*)(data.data() + 0x30);

		uint32_t phoff = *(uint32_t*)(data.data() + 0x1C);
		uint16_t phentsize = *(uint16_t*)(data.data() + 0x2A);
		uint16_t phnum = *(uint16_t*)(data.data() + 0x2C);

		for (uint16_t i = 0; i < phnum; i++) {
			// Type of segment.
			uint32_t type = *(uint32_t*)(data.data() + phoff + i * phentsize + 0x00);
			// Offset of the segment in the file image. 
			uint32_t offset = *(uint32_t*)(data.data() + phoff + i * phentsize + 0x04);
			// Virtual address of the segment in memory. 
			uint32_t vaddr = *(uint32_t*)(data.data() + phoff + i * phentsize + 0x08);
			// Size in bytes of the segment in the file image.
			uint32_t filesz = *(uint32_t*)(data.data() + phoff + i * phentsize + 0x10);
			// Size in bytes of the segment in memory.
			uint32_t memsz = *(uint32_t*)(data.data() + phoff + i * phentsize + 0x14);
			// Segment-dependent flags.
			uint32_t flags = *(uint32_t*)(data.data() + phoff + i * phentsize + 0x18);

			//if (flags & 0x04) {
			memory.clear(vaddr, memsz);
			memory.write(vaddr, data.data() + offset, filesz);
			//}
		}

		return entry;
	}

private:
	std::vector<uint8_t> data;
};
