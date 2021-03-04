#include "quadra_architecture.h"

#include <assert.h>

#include "elf_loader.h"

QuadraArchitecture::QuadraArchitecture(
	const std::string& fname,
	const std::string& targ,
	std::ostream* estream)
	: SleighArchitecture(fname, targ, estream)
{}

std::string QuadraArchitecture::return_register()
{
	switch(((ElfLoader*) loader)->machine()) {
		case ElfMachine::MIPS: return { "v0" };
		case ElfMachine::AMD64: return { "RAX" };
	}
	assert(0);
}

std::vector<std::string> QuadraArchitecture::syscall_argument_registers()
{
	switch(((ElfLoader*) loader)->machine()) {
		case ElfMachine::MIPS: return { "a0", "a1", "a2", "a3" };
		case ElfMachine::AMD64: return { "RDI", "RSI", "RDX", "R10", "R8", "R9" };
	}
	assert(0);
}

std::string QuadraArchitecture::syscall_return_register()
{
	switch(((ElfLoader*) loader)->machine()) {
		case ElfMachine::MIPS: return { "v0_lo" };
		case ElfMachine::AMD64: return { "RAX" };
	}
	assert(0);
}

std::map<int, SyscallInfo> QuadraArchitecture::syscalls()
{
	switch(((ElfLoader*) loader)->machine()) {
		case ElfMachine::MIPS: return {
			{4001, {"qsys_exit", PT_U32, {PT_U32}}},
			{4003, {"qsys_read", PT_U32, {PT_U32, PT_CHAR_PTR, PT_U32}}},
			{4004, {"qsys_write", PT_U32, {PT_U32, PT_CHAR_PTR, PT_U32}}},
			{4005, {"qsys_open", PT_U32, {PT_CHAR_PTR, PT_U32, PT_U32}}},
			{4006, {"qsys_close", PT_U32, {PT_U32}}}
		};
		case ElfMachine::AMD64: return {};
	}
	assert(0);
}

void QuadraArchitecture::buildLoader(DocumentStorage& store)
{
	collectSpecFiles(std::cerr);
	loader = new ElfLoader(getFilename());
}

void QuadraArchitecture::resolveArchitecture()
{
	SleighArchitecture::resolveArchitecture();
}

void QuadraArchitecture::postSpecFile()
{
	Architecture::postSpecFile();
	//((LoadImageBfd*) loader)->attachToSpace(getDefaultCodeSpace());
}

void QuadraArchitecture::saveXml(std::ostream& s) const
{
	assert(0);
}

void QuadraArchitecture::restoreXml(DocumentStorage& store)
{
	assert(0);
}
