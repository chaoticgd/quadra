#include "elf_loader.h"

#include <stdio.h>

ElfLoader::ElfLoader(std::string elf_path)
	: LoadImage(elf_path)
	, _file(elf_path, std::ios::binary)
{
	_file.seekg(0, std::ios::end);
	_file_size = _file.tellg();
	
	_ident = read_packed<ElfIdentHeader>(_file, 0);
	if(memcmp(_ident.magic, "\x7f\x45\x4c\x46", 4) != 0) {
		fprintf(stderr, "error: Not a valid ELF file.\n");
		exit(1);
	}
	switch(_ident.e_class) {
		case ElfIdentClass::B32: {
			ElfFileHeader32 header32 = read_packed<ElfFileHeader32>(_file);
			_header.type      = header32.type;
			_header.machine   = header32.machine;
			_header.version   = header32.version;
			_header.entry     = header32.entry;
			_header.phoff     = header32.phoff;
			_header.shoff     = header32.shoff;
			_header.flags     = header32.flags;
			_header.ehsize    = header32.ehsize;
			_header.phentsize = header32.phentsize;
			_header.phnum     = header32.phnum;
			_header.shentsize = header32.shentsize;
			_header.shnum     = header32.shnum;
			_header.shstrndx  = header32.shstrndx;
			
			_file.seekg(_header.phoff, std::ios::beg);
			for(uint32_t i = 0; i < _header.phnum; i++) {
				ElfProgramHeader32 segment32 = read_packed<ElfProgramHeader32>(_file);
				ElfProgramHeader64& segment = _segments.emplace_back();
				segment.type   = segment32.type;
				segment.flags  = segment32.flags;
				segment.offset = segment32.offset;
				segment.vaddr  = segment32.vaddr;
				segment.paddr  = segment32.paddr;
				segment.filesz = segment32.filesz;
				segment.memsz  = segment32.memsz;
				segment.align  = segment32.align;
			}
			break;
		}
		case ElfIdentClass::B64: {
			_header = read_packed<ElfFileHeader64>(_file);
			_segments = read_multiple_packed<ElfProgramHeader64>(_file, _header.phoff, _header.phnum);
			break;
		}
		default: {
			fprintf(stderr, "error Invalid e_class in ELF header.\n");
			exit(1);
		}
	}
}

std::string ElfLoader::getArchType() const
{
	return "MIPS:LE:32:default"; // HACK!
}

void ElfLoader::adjustVma(long adjust)
{
	fprintf(stderr, "error: ElfLoader::adjustVma(%lx) called\n", adjust);
	exit(1);
}

void ElfLoader::loadFill(uint1* ptr, int4 size, const Address& addr)
{
	uint64_t file_offset = file_offset_from_virtual_address(addr.getOffset());
	_file.seekg(file_offset);
	_file.read((char*) ptr, size);
	if(_file.gcount() != size) {
		fprintf(stderr, "error: Failed to load %x bytes from %08lx in ELF file.", size, file_offset);
		exit(1);
	}
}

uint64_t ElfLoader::file_offset_from_virtual_address(uint64_t virtual_address)
{
	for(ElfProgramHeader64& segment : _segments) {
		uint64_t top = segment.vaddr + segment.filesz;
		if(virtual_address >= segment.vaddr && virtual_address < top) {
			return segment.offset + (virtual_address - segment.vaddr);
		}
	}
	fprintf(stderr, "error: Tried to translate unmapped virtual address 0x%08lx.\n", virtual_address);
	exit(1);
	return 0;
}

uint64_t ElfLoader::top_of_segment_containing(uint64_t virtual_address) {
	for(ElfProgramHeader64& segment : _segments) {
		uint64_t top = segment.vaddr + segment.filesz;
		if(virtual_address >= segment.vaddr && virtual_address < top) {
			return top;
		}
	}
	fprintf(stderr, "error: Tried to calculate bounmds of segment containing unmapped virtual address 0x%08lx.\n", virtual_address);
	exit(1);
	return 0;
}
