#include "translator.h"

#include <decompile/cpp/funcdata.hh>

#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Verifier.h> // llvm::outs

QuadraTranslator::QuadraTranslator(QuadraArchitecture* arch)
	: _arch(arch)
	, _module("quadra", _context)
	, _builder(_context)
{
	std::map<VarnodeData, std::string> registers;
	_arch->translate->getAllRegisters(registers);
	for(auto& [varnode, _] : registers) {
		if(varnode.space->getName() == "register") {
			_register_space_size = std::max(_register_space_size, varnode.offset + varnode.size);
		}
	}
}

void QuadraTranslator::begin_function(QuadraFunction function)
{
	_function = std::move(function);
	
	auto blocks = _function.ghidra->getBasicBlocks().getList();
	assert(blocks.size() >= 1);
	_builder.SetInsertPoint(get_block(blocks[0])->llvm);
	
	auto registers_type = llvm::ArrayType::get(int_type(1), _register_space_size);
	llvm::AllocaInst* registers_alloca = _builder.CreateAlloca(registers_type, nullptr, "registers");
	_register_space = registers_alloca->getType()->getAddressSpace();
	auto register_ptr_type = llvm::PointerType::get(int_type(1), _register_space);
	_function.register_alloca = _builder.CreatePointerCast(registers_alloca, register_ptr_type, "");
	
	// HACK: Stack frame size fixed at 0x2000.
	auto stack_type = llvm::ArrayType::get(int_type(1), 0x2000);
	llvm::AllocaInst* stack_alloca = _builder.CreateAlloca(stack_type, nullptr, "stackframe");
	_function.stack_space = stack_alloca->getType()->getAddressSpace();
	auto stack_ptr_type = llvm::PointerType::get(int_type(1), _function.stack_space);
	_function.stack_alloca = _builder.CreatePointerCast(stack_alloca, stack_ptr_type, "");
}

void QuadraTranslator::end_function()
{
	Address address = _function.ghidra->getAddress();
	translated_functions.emplace(address, std::move(_function));
}

void QuadraTranslator::begin_block(const BlockBasic* gblock, llvm::Twine& name)
{
	_gblock = gblock;
	llvm::BasicBlock* lblock = get_block(gblock)->llvm;
	lblock->setName(name);
	_builder.SetInsertPoint(lblock);
}

void QuadraTranslator::end_block()
{
	QuadraBlock& block = _blocks[_gblock];
	if(!block.emitted_branch) {
		assert(_gblock->sizeOut() == 1);
		_builder.CreateBr(get_block(_gblock->getOut(0))->llvm);
	}
	_gblock = nullptr;
}

void QuadraTranslator::translate_pcodeop(const PcodeOp& op)
{
	assert(_gblock != nullptr && "QuadraTranslator::translate_pcodeop called outside a block!");
	
	QuadraBlock& block = _blocks[_gblock];
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
		case CPUI_LOAD: // 2
			assert(isize == 2);
			tmp1 = get_stack_memory(inputs[1], op.getOut()->getSize());
			output = _builder.CreateLoad(tmp1, "");
			break;
		case CPUI_STORE: // 3
			assert(isize == 3);
			tmp1 = get_stack_memory(inputs[1], op.getIn(2)->getSize());
			output = _builder.CreateStore(inputs[2], tmp1, false);
			break;
		case CPUI_BRANCH: // 4
			assert(isize == 1);
			assert(_gblock->sizeOut() == 1);
			output = _builder.CreateBr(get_block(_gblock->getOut(0))->llvm);
			block.emitted_branch = true;
			break;
		case CPUI_CBRANCH: // 5
			assert(isize == 2);
			assert(_gblock->sizeOut() == 2);
			tmp1 = _builder.CreateICmpNE(inputs[1], zero(op.getIn(1)->getSize()));
			output = _builder.CreateCondBr(
				tmp1,
				get_block(_gblock->getTrueOut())->llvm,
				get_block(_gblock->getFalseOut())->llvm);
			block.emitted_branch = true;
			break;
		case CPUI_CALL: { // 7
			FuncCallSpecs* call = _function.ghidra->getCallSpecs(&op);
			Address callee_addr = call->getEntryAddress();
			QuadraFunction* callee = get_function(callee_addr, nullptr);
			output = _builder.CreateCall(callee->llvm, {}, "", nullptr); 
			break;
		}
		case CPUI_CALLOTHER: // 9
			// HACK: For some reason my MIPS version of gcc is emitting a BREAK
			// instruction, which gets translated into a CALLOTHER. So lets
			// ignore that for now.
			return;
		case CPUI_RETURN: // 10
			assert(op.numInput() >= 2);
			output = _builder.CreateRet(inputs[1]);
			block.emitted_branch = true;
			break;
		case CPUI_INT_EQUAL: // 11
			tmp1 = _builder.CreateICmpEQ(inputs[0], inputs[1], "");
			output = _builder.CreateZExt(tmp1, int_type(1), ""); // i1 -> i8
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
			break;
		case CPUI_INT_XOR: // 26
			assert(isize == 2);
			assert(op.getIn(0)->getSize() == op.getIn(1)->getSize());
			output = _builder.CreateXor(inputs[0], inputs[1], "");
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
			assert(isize == 2);
			assert(op.getOut()->getSize() == op.getIn(0)->getSize());
			tmp1 = _builder.CreateZExt(inputs[1], int_type(op.getIn(0)->getSize()), "");
			output = _builder.CreateLShr(inputs[0], tmp1, "", false);
			break;
		case CPUI_INT_SRIGHT: // 31
			break;
		case CPUI_INT_MULT: // 32
			assert(isize == 2);
			output = _builder.CreateMul(inputs[0], inputs[1], "", false, false);
			break;
		case CPUI_INT_DIV: // 33
			assert(isize == 2);
			output = _builder.CreateUDiv(inputs[0], inputs[1]);
			break;
		case CPUI_INT_SDIV: // 34
			assert(isize == 2);
			output = _builder.CreateSDiv(inputs[0], inputs[1]);
			break;
		case CPUI_INT_REM: // 35
			assert(isize == 2);
			output = _builder.CreateURem(inputs[0], inputs[1]);
			break;
		case CPUI_INT_SREM: // 36
			assert(isize == 2);
			output = _builder.CreateSRem(inputs[0], inputs[1]);
			break;
		case CPUI_BOOL_NEGATE: // 37
			assert(isize == 1);
			assert(op.getOut()->getSize() == 1);
			assert(op.getIn(0)->getSize() == 1);
			tmp1 = _builder.CreateICmpEQ(inputs[0], zero(1), "");
			output = _builder.CreateZExt(tmp1, int_type(1), "");
			break;
		case CPUI_BOOL_XOR: // 38
		case CPUI_BOOL_AND: // 39
		case CPUI_BOOL_OR: // 40
			break;
		case CPUI_SUBPIECE: {// 63
			assert(isize == 2);
			assert(op.getIn(1)->getAddr().isConstant());
			llvm::APInt shift_val(op.getIn(0)->getSize() * 8, op.getIn(0)->getOffset() * 8, false);
			llvm::Value* shift = llvm::ConstantInt::get(_context, shift_val);
			llvm::Value* shifted = _builder.CreateAShr(inputs[0], shift, "", false);
			output = _builder.CreateTrunc(shifted, int_type(op.getOut()->getSize()), "");
			break;
		}
	}
	
	assert(output != nullptr && "Unimplemented or bad pcodeop!!!");
	if(op.getOut() != nullptr) {
		assert(op.getOut()->getSize() * 8 == output->getType()->getScalarSizeInBits());
		_builder.CreateStore(output, get_local(op.getOut()), false);
	}
}

void QuadraTranslator::print()
{
	_module.print(llvm::outs(), nullptr);
}

QuadraFunction* QuadraTranslator::get_function(Address address, const char* name)
{
	if(_function.ghidra != nullptr && _function.ghidra->getAddress() == address) {
		return &_function;
	}
	
	auto discovered_iter = discovered_functions.find(address);
	if(discovered_iter != discovered_functions.end()) {
		return &discovered_iter->second;
	}
	
	auto translated_iter = translated_functions.find(address);
	if(translated_iter != translated_functions.end()) {
		return &translated_iter->second;
	}
	
	std::stringstream name_ss;
	if(name != nullptr) {
		name_ss << name;
	} else {
		name_ss << "func_" << std::hex << address.getOffset();
	}
	
	QuadraFunction& function = discovered_functions[address];
	function.ghidra = std::make_unique<Funcdata>(name_ss.str(), _arch->symboltab->getGlobalScope(), address, 0);
	
	assert(_arch->allacts.getCurrent());
	_arch->allacts.getCurrent()->apply(*function.ghidra.get());
	assert(!function.ghidra->hasBadData() && "Function flowed into bad data!!!");
	
	PcodeOp* return_op = function.ghidra->getFirstReturnOp();
	assert(return_op->numInput() >= 2);
	Varnode* return_var = return_op->getIn(1);
	llvm::IntegerType* return_type = llvm::IntegerType::get(_context, return_var->getSize() * 8);
	llvm::FunctionType* func_type = llvm::FunctionType::get(return_type, false);
	function.llvm = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage, name_ss.str(), _module);
	return &function;
}

QuadraBlock* QuadraTranslator::get_block(const FlowBlock* gblock)
{
	const BlockBasic* basic_gblock = dynamic_cast<const BlockBasic*>(gblock);
	assert(basic_gblock != nullptr);
	auto iter = _blocks.find(basic_gblock);
	if(iter != _blocks.end()) {
		return &iter->second;
	}
	
	QuadraBlock& block = _blocks[basic_gblock];
	block.llvm = llvm::BasicBlock::Create(_context, "", _function.llvm, block.llvm);
	return &block;
}

llvm::Value* QuadraTranslator::get_input(const Varnode* var)
{
	llvm::Type* type = int_type(var->getSize());
	if(var->getAddr().isConstant()) {
		return llvm::ConstantInt::get(type, llvm::APInt(var->getSize() * 8, var->getOffset(), false));
	}
	
	// HACK: Assume the MIPS stack pointer is zero.
	if(var->getSpace()->getName() == "register" && var->getOffset() == 0xe8) {
		return zero(var->getSize());
	}
	
	return _builder.CreateLoad(get_local(var), "");
}

llvm::Value* QuadraTranslator::get_local(const Varnode* var)
{
	if(var->getSpace()->getName() == "register") {
		auto offset = llvm::ConstantInt::get(_context, llvm::APInt(32, var->getOffset(), false));
		llvm::Value* ptr8 = _builder.CreateGEP(_function.register_alloca, offset, "");
		auto ptr_type = llvm::PointerType::get(int_type(var->getSize()), _register_space);
		return _builder.CreatePointerCast(ptr8, ptr_type, "");
	}
	
	auto compare_varnodes = [](const Varnode* l, const Varnode* r) {
		return l->getSpace()->getIndex() == r->getSpace()->getIndex()
			&& l->getOffset() == r->getOffset()
			&& l->getSize() == r->getSize();
	};
	
	llvm::AllocaInst* local = nullptr;
	for(auto& [current_var, alloca] : _function.locals) {
		bool cmp = *current_var == *var;
		if(compare_varnodes(current_var, var)) {
			local = alloca;
		}
	}
	
	if(local == nullptr) {
		llvm::IRBuilder<> alloca_builder(
			&_function.llvm->getEntryBlock(),
			_function.llvm->getEntryBlock().begin());
		std::stringstream name;
		name << _arch->translate->getRegisterName(
			var->getSpace(), var->getOffset(), var->getSize());
		if(name.str().size() > 0) {
			name << "_";
		}
		name << "l_0x" << std::hex << var->getOffset();
		name << "_" << var->getSpace()->getName();
		local = alloca_builder.CreateAlloca(int_type(var->getSize()), 0, name.str());
		_function.locals[var] = local;
	}
	
	return local;
}

llvm::Value* QuadraTranslator::get_stack_memory(llvm::Value* offset, int4 size_bytes)
{
	llvm::Value* ptr = _builder.CreateGEP(_function.stack_alloca, offset, "");
	auto ptr_type = llvm::PointerType::get(int_type(size_bytes), _function.stack_space);
	return _builder.CreatePointerCast(ptr, ptr_type, "");
}

llvm::Value* QuadraTranslator::zero(int4 bytes)
{
	return llvm::ConstantInt::get(int_type(bytes), llvm::APInt(bytes * 8, 0, false));
}

llvm::Type* QuadraTranslator::int_type(int4 bytes)
{
	return llvm::IntegerType::get(_context, bytes * 8);
}
