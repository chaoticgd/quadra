#ifndef _QUADRA_TRANSLATOR_H
#define _QUADRA_TRANSLATOR_H

#include <decompile/cpp/sleigh.hh>

#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>

// Translates from pcode to LLVM IR.
class QuadraTranslator {
public:
	QuadraTranslator(Sleigh* sleigh);

	void translate_pcodeop(
		const Address& addr,
		OpCode opc,
		VarnodeData* outvar,
		VarnodeData* vars,
		int4 isize);
	
	void print();
	
private:
	llvm::Value* value_from_varnode(VarnodeData var); // Convert a Ghidra varnode to an LLVM value.
	llvm::Value* zero(int4 bytes);

	Sleigh* _sleigh;
	llvm::LLVMContext _context;
	llvm::Module _module;
	llvm::IRBuilder<> _builder;
	std::map<VarnodeData, llvm::Value*> _values; // Map of varnodes that currently have an associated LLVM value.
};

#endif
