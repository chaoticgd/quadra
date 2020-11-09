#ifndef _QUADRA_TRANSLATOR_H
#define _QUADRA_TRANSLATOR_H

#include <decompile/cpp/sleigh.hh>

#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>

#include "quadra_architecture.h"

// Translates from pcode to LLVM IR.
//
// Basic blocks and pcode ops are fed to it using an immediate-style API, and
// from that it generates all the necessary LLVM calls.
class QuadraTranslator {
public:
	QuadraTranslator(QuadraArchitecture* arch, Funcdata* funcdata);
	
	void begin_function(const Funcdata* gfunction);
	void end_function();
	void begin_block(const BlockBasic& gblock, llvm::Twine& name);
	void end_block(const BlockBasic& gblock);
	void translate_pcodeop(const PcodeOp& op);
	
	void print();
	
private:
	llvm::BasicBlock* get_block(const FlowBlock* gblock);
	llvm::Value* get_input(const Varnode* var); // Convert a Ghidra varnode to an LLVM value.
	llvm::Value* get_local(const Varnode* var); // Create an alloca for a varnode if it doesn't already exist, then return it.
	llvm::Value* zero(int4 bytes);
	llvm::Type* int_type(int4 bytes);

	QuadraArchitecture* _arch;
	Funcdata* _funcdata;
	
	llvm::LLVMContext _context;
	llvm::Module _module;
	llvm::IRBuilder<> _builder;
	llvm::Function* _function;
	
	struct BlockData {
		llvm::BasicBlock* lblock;
		bool emitted_branch = false;
	};
	const BlockBasic* _block;
	std::map<const BlockBasic*, BlockData> _blocks;
	std::map<const Varnode*, llvm::AllocaInst*> _locals;
	
	uintb _register_space_size;
	unsigned int _register_space;
	llvm::Value* _register_alloca;
};

#endif
