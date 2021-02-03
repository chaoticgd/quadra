#ifndef _QUADRA_TRANSLATOR_H
#define _QUADRA_TRANSLATOR_H

#include <decompile/cpp/sleigh.hh>

#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>

#include "quadra_architecture.h"

struct QuadraBlock {
	llvm::BasicBlock* llvm;
	bool emitted_branch = false;
};

struct QuadraFunction {
	Funcdata* ghidra;
	llvm::Function* llvm = nullptr;
	std::map<const Varnode*, llvm::AllocaInst*> locals;
	llvm::Value* register_alloca = nullptr;
	unsigned int stack_space = 0;
	llvm::Value* stack_alloca = nullptr;
};

static const bool STORE_REGISTERS_IN_GLOBAL = true;

// Translates from pcode to LLVM IR.
//
// Basic blocks and pcode ops are fed to it using an immediate-style API, and
// from that it generates all the necessary LLVM calls.
class QuadraTranslator {
public:
	QuadraTranslator(QuadraArchitecture* arch);
	
	void begin_function(QuadraFunction function);
	void end_function();
	void begin_block(const BlockBasic* gblock, llvm::Twine& name);
	void end_block();
	void translate_pcodeop(const PcodeOp& op);
	
	void print();
	
	QuadraFunction* get_function(Address address, const char* name = nullptr);
	
	std::map<Address, QuadraFunction> discovered_functions;
	std::map<Address, QuadraFunction> translated_functions;
	
private:
	QuadraBlock* get_block(const FlowBlock* gblock);
	llvm::Value* get_input(const Varnode* var); // Convert a Ghidra varnode to an LLVM value.
	llvm::Value* get_local(const Varnode* var); // Create an alloca for a varnode if it doesn't already exist, then return it.
	llvm::Value* get_register(uintb offset, int4 size_bytes); // Get a pointer to a register.
	llvm::Value* decompress_pointer(llvm::Value* val, llvm::Value* hi, llvm::Type* ptr_type); // Take a truncated pointer, add on the hi 32 bits pf hi.
	
	llvm::Value* register_storage();
	
	llvm::Value* zero(int4 bytes);
	llvm::Type* int_type(int4 bytes);
	
	void create_printf_int(const char* fmt, llvm::Value* val);
	
	llvm::Function* create_syscall_dispatcher();

	QuadraArchitecture* _arch;
	
	llvm::LLVMContext _context;
	llvm::Module _module;
	llvm::IRBuilder<> _builder;
	
	QuadraFunction _function;
	const BlockBasic* _gblock = nullptr;
	std::map<const BlockBasic*, QuadraBlock> _blocks;
	
	uintb _register_space_size = 0;
	unsigned int _register_space;
	llvm::Value* _registers_global;
	
	llvm::Function* _syscall_dispatcher = nullptr;
};

#endif
