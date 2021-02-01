#include <iostream>
#include <filesystem>

#include <decompile/cpp/libdecomp.hh>
#include <decompile/cpp/loadimage.hh>
#include <decompile/cpp/database.hh>
#include <decompile/cpp/funcdata.hh>
#include <decompile/cpp/flow.hh>

#include "quadra_architecture.h"
#include "elf_loader.h"
#include "translator.h"

namespace fs = std::filesystem;

void disassemble_pcodeop(const Translate& translate, size_t index, const PcodeOp& op);
void print_vardata(const Translate& translate, const Varnode& var);

int main(int argc, char** argv)
{
	const char* ghidra_dir = getenv("GHIDRA_DIR");
	if(ghidra_dir == nullptr) {
		fprintf(stderr, "GHIDRA_DIR enviroment variable required but not provided.\n");
		exit(1);
	}
	
	startDecompilerLibrary(ghidra_dir);
	
	if(argc < 2) {
		printf("usage: GHIDRA_DIR=/path/to/ghidra ./quadra /path/to/executable\n");
		exit(1);
	}
	const char* binary_path = argv[1];
	QuadraArchitecture arch(binary_path, "", &std::cerr);
	DocumentStorage document_storage;
	try {
		arch.init(document_storage);
	} catch(SleighError& err) {
		fprintf(stderr, "Failed to load SLEIGH specification. Did you forget to compile it?\n");
		fprintf(stderr, "%s\n", err.explain.c_str());
		exit(1);
	}
	
	QuadraTranslator pcode_to_llvm(&arch);
	
	uint64_t entry_point = ((ElfLoader*) arch.loader)->entry_point();
	Address entry_point_addr(arch.translate->getDefaultCodeSpace(), entry_point);
	
	// Create the Ghidra/LLVM function objects.
	pcode_to_llvm.get_function(entry_point_addr, "main");
	
	while(pcode_to_llvm.discovered_functions.size() > 0) {
		Address address = pcode_to_llvm.discovered_functions.begin()->first;
		auto node_handle = pcode_to_llvm.discovered_functions.extract(address);
		QuadraFunction function = std::move(node_handle.mapped());
		
		const auto blocks = function.ghidra->getBasicBlocks();
		pcode_to_llvm.begin_function(std::move(function));
		
		fprintf(stderr, "%s() {\n", function.llvm->getName().data());
		
		for(const FlowBlock* block : blocks.getList()) {
			const BlockBasic* basic = dynamic_cast<const BlockBasic*>(block);
			assert(basic != nullptr); // We're not doing any control flow recovery, this should never happen.
			
			char block_name[1024];
			snprintf(block_name, 1024, "block_%lx", basic->getEntryAddr().getOffset() - address.getOffset());
			fprintf(stderr, "%s:\n", block_name);
			
			llvm::Twine block_twine(block_name);
			pcode_to_llvm.begin_block(basic, block_twine);
			Address last_address;
			uintm first_time = 0;
			for(auto iter = basic->beginOp(); iter != basic->endOp(); iter++) {
				const PcodeOp& op = **iter;
				
				if(op.getAddr() != last_address) {
					first_time = op.getTime();
				}
				last_address = op.getAddr();
				
				disassemble_pcodeop(*arch.translate, op.getTime() - first_time, op);
				pcode_to_llvm.translate_pcodeop(op);
			}
			pcode_to_llvm.end_block();
		}
		
		pcode_to_llvm.end_function();
		
		fprintf(stderr, "}\n");
	}
	
	pcode_to_llvm.print();
	
	shutdownDecompilerLibrary(); // Does nothing.
}

void disassemble_pcodeop(const Translate& translate, size_t index, const PcodeOp& op)
{
	fprintf(stderr, "\t %08lx:%04lx\t", op.getAddr().getOffset(), index);
	if(op.getOut() != nullptr) {
		print_vardata(translate, *op.getOut());
		std::cerr << " = ";
	}
	std::cerr << get_opname(op.code());
	for(int4 i = 0; i < op.numInput(); i++) {
		std::cerr << ' ';
		print_vardata(translate, *op.getIn(i));
	}
	std::cerr << endl;
}

void print_vardata(const Translate& translate, const Varnode& var)
{
	if(var.getSpace()->getName() == "register") {
		auto name = translate.getRegisterName(var.getSpace(), var.getOffset(), var.getSize());
		if(name.size() > 0) {
			std::cerr << name << ':' << var.getSize();
			return;
		}
	}
	std::cerr << '(' << var.getSpace()->getName() << ',';
	var.getSpace()->printOffset(std::cerr, var.getOffset());
	std::cerr << ',' << dec << var.getSize() << ')';
}
