#include "translator.h"

#include <decompile/cpp/funcdata.hh>

#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Verifier.h> // llvm::outs

QuadraTranslator::QuadraTranslator(QuadraArchitecture* arch, Funcdata* funcdata)
	: _arch(arch)
	, _funcdata(funcdata)
	, _module("quadra", _context)
	, _builder(_context)
{
	llvm::FunctionType* func_type = llvm::FunctionType::get(llvm::Type::getInt32Ty(_context), false);
	_function = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage, "main", _module);
}

void QuadraTranslator::begin_block(const BlockBasic& gblock, llvm::Twine& name)
{
	_block = &gblock;
	llvm::BasicBlock* lblock = get_block(&gblock);
	lblock->setName(name);
	_builder.SetInsertPoint(lblock);
}

void QuadraTranslator::end_block(const BlockBasic& gblock)
{
	BlockData& block = _blocks[&gblock];
	if(!block.emitted_branch) {
		assert(gblock.sizeOut() == 1);
		_builder.CreateBr(get_block(gblock.getOut(0)));
	}
	_block = nullptr;
}

void QuadraTranslator::translate_pcodeop(const PcodeOp& op)
{
	if(_block == nullptr) {
		fprintf(stderr, "QuadraTranslator::translate_pcodeop called outside a block!!!\n");
		exit(1);
	}
	BlockData& data = _blocks[_block];
	int4 isize = op.numInput();
	
	llvm::Value* inputs[3];
	assert(isize >= 0 && isize <= 3);
	for(int4 i = 0; i < isize; i++) {
		inputs[i] = get_input(op.getIn(i));
	}
	
	llvm::Value* output = nullptr;
	llvm::Type* type = nullptr;
	llvm::Value* tmp1 = nullptr;
	switch(op.code()) {
		case CPUI_COPY: // 1
			assert(isize == 1);
			assert(op.getOut()->getSize() == op.getIn(0)->getSize());
			output = inputs[0];
			break;
		case CPUI_CBRANCH: // 5
			assert(isize == 2);
			assert(_block->sizeOut() == 2);
			tmp1 = _builder.CreateICmpNE(inputs[1], zero(op.getIn(1)->getSize()));
			output = _builder.CreateCondBr(
				tmp1,
				get_block(_block->getTrueOut()),
				get_block(_block->getFalseOut()));
			data.emitted_branch = true;
			break;
		case CPUI_RETURN: // 10
			assert(isize == 1);
			// HACK!
			tmp1 = get_input(_funcdata->newVarnode(
				_arch->getSpaceByName("register")->getIndex(),
				Address(_arch->getSpaceByName("register"), _arch->translate->getRegister("v0").offset)));
			output = _builder.CreateRet(tmp1);
			data.emitted_branch = true;
			break;
		case CPUI_INT_EQUAL: // 11
			break;
		case CPUI_INT_NOTEQUAL: // 12
			assert(isize == 2);
			assert(op.getIn(0)->getSize() == op.getIn(1)->getSize());
			tmp1 = _builder.CreateICmpNE(inputs[0], inputs[1], "");
			output = _builder.CreateZExt(tmp1, int_type(1), ""); // i1 -> i8
			break;
		case CPUI_INT_SLESS: // 13
			assert(isize == 2);
			tmp1 = _builder.CreateICmpSLT(inputs[0], inputs[1], "");
			output = _builder.CreateZExt(tmp1, int_type(1), ""); // i1 -> i8
			break;
		case CPUI_INT_SLESSEQUAL: // 14
			assert(isize == 2);
			tmp1 = _builder.CreateICmpSLE(inputs[0], inputs[1], "");
			output = _builder.CreateZExt(tmp1, int_type(1), ""); // i1 -> i8
			break;
		case CPUI_INT_LESS: // 15
		case CPUI_INT_LESSEQUAL: // 16
			break;
		case CPUI_INT_ZEXT: // 17
			assert(isize == 1);
			type = int_type(op.getOut()->getSize());
			output = _builder.CreateZExt(inputs[0], type, "");
			break;
		case CPUI_INT_SEXT: // 18
			assert(isize == 1);
			type = int_type(op.getOut()->getSize());
			output = _builder.CreateSExt(inputs[0], type, "");
			break;
		case CPUI_INT_ADD: // 19
			assert(isize == 2);
			output = _builder.CreateAdd(inputs[0], inputs[1], "", false, false);
			break;
		case CPUI_INT_SUB: // 20
			assert(isize == 2);
			output = _builder.CreateSub(inputs[0], inputs[1], "", false, false);
			break;
		case CPUI_INT_CARRY: // 21
		case CPUI_INT_SCARRY: // 22
		case CPUI_INT_SBORROW: // 23
			break;
		case CPUI_INT_2COMP: // 24
			assert(isize == 1);
			output = _builder.CreateSub(zero(op.getIn(0)->getSize()), inputs[0], "", false, false);
			break;
		case CPUI_INT_NEGATE: // 25
		case CPUI_INT_XOR: // 26
			break;
		case CPUI_INT_AND: // 27
			assert(isize == 2);
			output = _builder.CreateAnd(inputs[0], inputs[1], "");
			break;
		case CPUI_INT_OR: // 28
			assert(isize == 2);
			output = _builder.CreateOr(inputs[0], inputs[1], "");
			break;
		case CPUI_INT_LEFT: // 29
			assert(isize == 2);
			output = _builder.CreateShl(inputs[0], inputs[1], "", false, false);
			break;
		case CPUI_INT_RIGHT: // 30
		case CPUI_INT_SRIGHT: // 31
			break;
		case CPUI_INT_MULT: // 32
			assert(isize == 2);
			output = _builder.CreateMul(inputs[0], inputs[1], "", false, false);
			break;
		case CPUI_INT_DIV: // 33
		case CPUI_INT_SDIV: // 34
		case CPUI_INT_REM: // 35
		case CPUI_INT_SREM: // 36
			break;
		case CPUI_SUBPIECE: // 63
			assert(isize == 2);
			assert(op.getIn(1)->getAddr().isConstant());
			tmp1 = _builder.CreateAShr(inputs[0], inputs[1], "", false);
			output = _builder.CreateTrunc(tmp1, int_type(op.getOut()->getSize()), "");
			break;
	}
	
	if(output != nullptr) {
		if(op.getOut() != nullptr) {
			assert(op.getOut()->getSize() * 8 == output->getType()->getScalarSizeInBits());
			_builder.CreateStore(output, get_local(op.getOut()), false);
		}
	} else {
		fprintf(stderr, "warning: Unimplemented or bad pcodeop!!!\n");
	}
}
void print_var(const Varnode& var)
{
	std::cerr << '(' << var.getSpace()->getName() << ',';
	var.getSpace()->printOffset(std::cerr, var.getOffset());
	std::cerr << ',' << dec << var.getSize() << ')';
}

void QuadraTranslator::print()
{
	_module.print(llvm::outs(), nullptr);
}

llvm::BasicBlock* QuadraTranslator::get_block(const FlowBlock* gblock)
{
	const BlockBasic* basic_gblock = dynamic_cast<const BlockBasic*>(gblock);
	assert(basic_gblock != nullptr);
	auto iter = _blocks.find(basic_gblock);
	if(iter != _blocks.end()) {
		return iter->second.lblock;
	}
	llvm::BasicBlock* lblock = llvm::BasicBlock::Create(_context, "", _function);
	_blocks[basic_gblock] = { lblock };
	return lblock;
}

llvm::Value* QuadraTranslator::get_input(const Varnode* var)
{
	BlockData& data = _blocks[_block];
	
	llvm::Type* type = int_type(var->getSize());
	if(var->getAddr().isConstant()) {
		return llvm::ConstantInt::get(type, llvm::APInt(var->getSize() * 8, var->getOffset(), false));
	}
	
	return _builder.CreateLoad(get_local(var), "");
}

llvm::AllocaInst* QuadraTranslator::get_local(const Varnode* var) {
	auto compare_varnodes = [](const Varnode* l, const Varnode* r) {
		return l->getSpace()->getIndex() == r->getSpace()->getIndex()
			&& l->getOffset() == r->getOffset()
			&& l->getSize() == r->getSize();
	};
	
	llvm::AllocaInst* local = nullptr;
	for(auto& [current_var, alloca] : _locals) {
		bool cmp = *current_var == *var;
		if(compare_varnodes(current_var, var)) {
			local = alloca;
		}
	}
	
	if(local == nullptr) {
		llvm::IRBuilder<> alloca_builder(
			&_function->getEntryBlock(),
			_function->getEntryBlock().begin());
		std::stringstream name;
		name << _arch->translate->getRegisterName(
			var->getSpace(), var->getOffset(), var->getSize());
		if(name.str().size() > 0) {
			name << "_";
		}
		name << "l_0x" << std::hex << var->getOffset();
		name << "_" << var->getSpace()->getName();
		local = alloca_builder.CreateAlloca(int_type(var->getSize()), 0, name.str());
		_locals[var] = local;
	}
	
	return local;
}

llvm::Value* QuadraTranslator::zero(int4 bytes)
{
	return llvm::ConstantInt::get(int_type(bytes), llvm::APInt(bytes * 8, 0, false));
}

llvm::Type* QuadraTranslator::int_type(int4 bytes)
{
	return llvm::IntegerType::get(_context, bytes * 8);
}
