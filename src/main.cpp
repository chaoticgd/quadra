#include <iostream>
#include <filesystem>

#include <decompile/cpp/loadimage.hh>
#include <decompile/cpp/sleigh.hh>

#include "elf_loader.h"
#include "translator.h"

namespace fs = std::filesystem;

void print_vardata(VarnodeData &data)
{
	std::cerr << '(' << data.space->getName() << ',';
	data.space->printOffset(std::cerr, data.offset);
	std::cerr << ',' << dec << data.size << ')';
}

template <typename F>
void for_each_pcodeop(Sleigh& translator, Address addr, Address last_addr, F callback);

std::string compile_sla_file_if_missing(std::string slaspec_path);

int main(int argc, char** argv)
{
	std::string slaspec_file = argv[1];
	std::string sla_file = compile_sla_file_if_missing(slaspec_file);
	
	ElfLoader loader(argv[2]);
	
	ContextInternal context;
	
	Sleigh machine_code_to_pcode(&loader, &context);
	
	DocumentStorage document_storage;
	Element* sleigh_root = document_storage.openDocument(sla_file)->getRoot();
	document_storage.registerTag(sleigh_root);
	machine_code_to_pcode.initialize(document_storage);
	
	uint64_t entry_point = loader.entry_point();
	uint64_t entry_segment_top = loader.top_of_segment_containing(entry_point);
	Address addr(machine_code_to_pcode.getDefaultCodeSpace(), entry_point);
	Address last_addr(machine_code_to_pcode.getDefaultCodeSpace(), entry_segment_top);
	
	QuadraTranslator pcode_to_llvm(&machine_code_to_pcode);
	
	for_each_pcodeop(machine_code_to_pcode, addr, last_addr, [&](
		const Address& addr,
		OpCode opc,
		VarnodeData* outvar,
		VarnodeData* vars,
		int4 isize)
	{
		// Print disassembly.
		if(outvar != nullptr) {
			print_vardata(*outvar);
			std::cerr << " = ";
		}
		std::cerr << get_opname(opc);
		for(int4 i = 0; i < isize; i++) {
			std::cerr << ' ';
			print_vardata(vars[i]);
		}
		std::cerr << endl;
		
		// Convert to LLVM IR.
		pcode_to_llvm.translate_pcodeop(addr, opc, outvar, vars, isize);
	});
	
	pcode_to_llvm.print();
}

template <typename F>
void for_each_pcodeop(Sleigh& translator, Address addr, Address last_addr, F callback)
{
	class FunPcodeEmit : public PcodeEmit {
	public:
		FunPcodeEmit(F callback) : _callback(callback) {}

		void dump(
			const Address& addr,
			OpCode opc,
			VarnodeData* outvar,
			VarnodeData* vars,
			int4 isize) override
		{
			_callback(addr, opc, outvar, vars, isize);
		}

	private:
		F _callback;
	};
	
	FunPcodeEmit emit(callback);
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
