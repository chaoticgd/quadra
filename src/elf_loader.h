#ifndef _QUADRA_ELF_LOADER_H
#define _QUADRA_ELF_LOADER_H

#include <iostream>
#include <inttypes.h>
#include <decompile/cpp/loadimage.hh>

#include "packed_struct.h"

#include <elf.h>

enum class ElfIdentClass : uint8_t {
	B32 = 0x1,
	B64 = 0x2
};

enum class ElfFileType : uint16_t {
	NONE   = 0x00,
	REL    = 0x01,
	EXEC   = 0x02,
	DYN    = 0x03,
	CORE   = 0x04,
	LOOS   = 0xfe00,
	HIOS   = 0xfeff,
	LOPROC = 0xff00,
	HIPROC = 0xffff
};

enum class ElfMachine : uint16_t {
	MIPS  = 0x08,
	AMD64 = 0x3e
};

packed_struct(ElfIdentHeader,
	uint8_t magic[4];      // 0x0 7f 45 4c 46
	ElfIdentClass e_class; // 0x4
	uint8_t endianess;     // 0x5
	uint8_t version;       // 0x6
	uint8_t os_abi;        // 0x7
	uint8_t abi_version;   // 0x8
	uint8_t pad[7];        // 0x9
)

packed_struct(ElfFileHeader32,
	ElfFileType type;     // 0x10
	ElfMachine machine;   // 0x12
	uint32_t version;     // 0x14
	uint32_t entry;       // 0x18
	uint32_t phoff;       // 0x1c
	uint32_t shoff;       // 0x20
	uint32_t flags;       // 0x24
	uint16_t ehsize;      // 0x28
	uint16_t phentsize;   // 0x2a
	uint16_t phnum;       // 0x2c
	uint16_t shentsize;   // 0x2e
	uint16_t shnum;       // 0x30
	uint16_t shstrndx;    // 0x32
)

packed_struct(ElfFileHeader64,
	ElfFileType type;     // 0x10
	ElfMachine machine;   // 0x12
	uint32_t version;     // 0x14
	uint64_t entry;       // 0x18
	uint64_t phoff;       // 0x20
	uint64_t shoff;       // 0x28
	uint32_t flags;       // 0x30
	uint16_t ehsize;      // 0x34
	uint16_t phentsize;   // 0x36
	uint16_t phnum;       // 0x38
	uint16_t shentsize;   // 0x3a
	uint16_t shnum;       // 0x3c
	uint16_t shstrndx;    // 0x3e
)

packed_struct(ElfProgramHeader32,
	uint32_t type;   // 0x0
	uint32_t offset; // 0x4
	uint32_t vaddr;  // 0x8
	uint32_t paddr;  // 0xc
	uint32_t filesz; // 0x10
	uint32_t memsz;  // 0x14
	uint32_t flags;  // 0x18
	uint32_t align;  // 0x1c
)

packed_struct(ElfProgramHeader64,
	uint32_t type;   // 0x0
	uint32_t flags;  // 0x4
	uint64_t offset; // 0x8
	uint64_t vaddr;  // 0x10
	uint64_t paddr;  // 0x18
	uint64_t filesz; // 0x20
	uint64_t memsz;  // 0x28
	uint64_t align;  // 0x30
)

class ElfLoader : public LoadImage {
public:
	ElfLoader(std::string elf_path);
	~ElfLoader() override {}
	void loadFill(uint1* ptr, int4 size, const Address& addr) override;
	string getArchType() const override;
	void adjustVma(long adjust) override;
	
	uint64_t entry_point() const { return _header.entry; }
	ElfMachine machine() const { return _header.machine; }
	uint64_t file_offset_from_virtual_address(uint64_t virtual_address);
	uint64_t top_of_segment_containing(uint64_t virtual_address);

private:
	std::ifstream _file;
	size_t _file_size;
	ElfIdentHeader _ident;
	ElfFileHeader64 _header;
	std::vector<ElfProgramHeader64> _segments;
};

#endif
