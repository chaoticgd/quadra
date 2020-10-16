#include <iostream>
#include <filesystem>

#include <decompile/cpp/loadimage.hh>
#include <decompile/cpp/sleigh.hh>

#include "elf_loader.h"

namespace fs = std::filesystem;

// Most of this was adapted from sleighexample.cc.

void print_vardata(VarnodeData &data)
{
	std::cout << '(' << data.space->getName() << ',';
	data.space->printOffset(std::cout, data.offset);
	std::cout << ',' << dec << data.size << ')';
}

class PcodeRawOut : public PcodeEmit {
public:
	void dump(const Address &addr, OpCode opc, VarnodeData *outvar, VarnodeData *vars, int4 isize) override
	{
		if(outvar != (VarnodeData *)0) {
			print_vardata(*outvar);
			std::cout << " = ";
		}
		std::cout << get_opname(opc);
		// Possibly check for a code reference or a space reference
		for(int4 i=0;i<isize;++i) {
			cout << ' ';
			print_vardata(vars[i]);
		}
		cout << endl;
	}
};

std::string compile_sla_file_if_missing(std::string slaspec_path);

int main(int argc, char** argv)
{
	std::string slaspec_file = argv[1];
	std::string sla_file = compile_sla_file_if_missing(slaspec_file);
	
	ElfLoader loader(argv[2]);
	
	ContextInternal context;
	
	Sleigh translator(&loader, &context);
	
	DocumentStorage document_storage;
	Element* sleigh_root = document_storage.openDocument(sla_file)->getRoot();
	document_storage.registerTag(sleigh_root);
	translator.initialize(document_storage);
	
	uint64_t entry_point = loader.entry_point();
	uint64_t entry_segment_top = loader.top_of_segment_containing(entry_point);
	Address addr(translator.getDefaultCodeSpace(), entry_point);
	Address last_addr(translator.getDefaultCodeSpace(), entry_segment_top);
	
	PcodeRawOut emit;
	while(addr < last_addr) {
		int4 length = translator.oneInstruction(emit, addr);
		addr = addr + length;
	}
}

std::string compile_sla_file_if_missing(std::string slaspec_path)
{
	static const char* SLASPEC_EXTENSION = ".slaspec";
	if(slaspec_path.find(SLASPEC_EXTENSION) != slaspec_path.size() - strlen(SLASPEC_EXTENSION)) {
		fprintf(stderr, "error: The provided file is not a .slaspec file.\n");
		exit(1);
	}
	
	std::string sla_path = slaspec_path.substr(0, slaspec_path.size() - 4);
	if(!fs::exists(sla_path)) {
		std::string command = "./sleigh_opt " + slaspec_path;
		if(system(command.c_str()) != 0 || !fs::exists(sla_path)) {
			fprintf(stderr, "error: Failed to compile provided .slaspec file.\n");
			exit(1);
		}
	}
	
	return sla_path;
}
