#include "translator.h"

#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Verifier.h> // llvm::outs

QuadraTranslator::QuadraTranslator(Sleigh* sleigh)
	: _sleigh(sleigh)
	, _module("quadra", _context)
	, _builder(_context)
{
	llvm::FunctionType* func_type = llvm::FunctionType::get(llvm::Type::getInt32Ty(_context), false);
	llvm::Function* func = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage, "f", _module);
	llvm::BasicBlock* bb = llvm::BasicBlock::Create(_context, "", func);
	_builder.SetInsertPoint(bb);
}

void QuadraTranslator::translate_pcodeop(
	const Address& addr,
	OpCode opc,
	VarnodeData* outvar,
	VarnodeData* vars,
	int4 isize)
{
	llvm::Value* inputs[3];
	assert(isize >= 0 && isize <= 3);
	for(int4 i = 0; i < isize; i++) {
		inputs[i] = value_from_varnode(vars[i]);
	}
	
	llvm::Value* output = nullptr;
	
	switch(opc) {
		case CPUI_COPY: { // 1
			assert(isize == 1);
			assert(outvar->size == vars[0].size);
			output = inputs[0];
			break;
		}
		case CPUI_RETURN: { // 10
			// HACK!
			VarnodeData mips_v0 {
				vars[0].space /* register space */, 0x8, 0x4
			};
			_builder.CreateRet(value_from_varnode(mips_v0));
			break;
		}
		case CPUI_INT_EQUAL: { // 11
			break;
		}
		case CPUI_INT_NOTEQUAL: { // 12
			llvm::Value* eq = _builder.CreateICmpEQ(inputs[0], inputs[1], "cmp");
			output = _builder.CreateNot(eq, "neq");
			break;
		}
		case CPUI_INT_SLESS: { // 13
			break;
		}
		case CPUI_INT_SLESSEQUAL: { // 14
			break;
		}
		case CPUI_INT_LESS: { // 15
			break;
		}
		case CPUI_INT_LESSEQUAL: { // 16
			break;
		}
		case CPUI_INT_ZEXT: { // 17
			break;
		}
		case CPUI_INT_SEXT: { // 18
			break;
		}
		case CPUI_INT_ADD: { // 19
			output = _builder.CreateAdd(inputs[0], inputs[1], "add", false, false);
			break;
		}
		case CPUI_INT_SUB: { // 20
			output = _builder.CreateSub(inputs[0], inputs[1], "sub", false, false);
			break;
		}
		case CPUI_INT_CARRY: { // 21
			break;
		}
		case CPUI_INT_SCARRY: { // 22
			break;
		}
		case CPUI_INT_SBORROW: { // 23
			break;
		}
		case CPUI_INT_2COMP: { // 24
			output = _builder.CreateSub(zero(vars[0].size), inputs[0], "2comp", false, false);
			break;
		}
		case CPUI_INT_NEGATE: { // 25
			break;
		}
		case CPUI_INT_XOR: { // 26
			break;
		}
		case CPUI_INT_AND: { // 27
			output = _builder.CreateAnd(inputs[0], inputs[1], "and");
			break;
		}
		case CPUI_INT_OR: { // 28
			output = _builder.CreateOr(inputs[0], inputs[1], "or");
			break;
		}
		case CPUI_INT_LEFT: { // 29
			output = _builder.CreateShl(inputs[0], inputs[1], "left", false, false);
			break;
		}
	}
	
	if(output != nullptr) {
		_values[*outvar] = output;
	} else {
		fprintf(stderr, "warning: Unimplemented or bad pcodeop!!!\n");
	}
}

void QuadraTranslator::print()
{
	_module.print(llvm::outs(), nullptr);
}

llvm::Value* QuadraTranslator::value_from_varnode(VarnodeData var)
{
	auto existing_value = _values.find(var);
	if(existing_value != _values.end()) {
		return existing_value->second;
	}
	
	if(var.getAddr().isConstant()) {
		llvm::Type* type = llvm::IntegerType::get(_context, var.size * 8);
		llvm::Value* constant = llvm::ConstantInt::get(type, llvm::APInt(var.size * 8, var.offset, false));
		_values[var] = constant;
		return constant;
	}
	
	fprintf(stderr, "warning: Input varnode (%s,0x%lx,0x%x) for register %s not previously referenced!\n",
		var.space->getName().c_str(), var.offset, var.size,
		_sleigh->getRegisterName(var.space, var.offset, var.size).c_str());
		
	// Return a dummy value.
	return zero(var.size);
}

llvm::Value* QuadraTranslator::zero(int4 bytes)
{
	return llvm::ConstantInt::get(llvm::IntegerType::get(_context, bytes * 8), llvm::APInt(bytes * 8, 0, false));
}