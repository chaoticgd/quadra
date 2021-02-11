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
	
	if(STORE_REGISTERS_IN_GLOBAL) {
		auto registers_type = llvm::ArrayType::get(int_type(1), _register_space_size);
		llvm::GlobalVariable* registers_global = new llvm::GlobalVariable(
			_module,
			registers_type,
			false,
			llvm::GlobalValue::CommonLinkage,
			llvm::Constant::getNullValue(registers_type),
			"registers");
		_register_space = registers_global->getType()->getAddressSpace();
		auto register_ptr_type = llvm::PointerType::get(int_type(1), _register_space);
		_registers_global = _builder.CreatePointerCast(registers_global, register_ptr_type, "");
		
		_syscall_dispatcher = create_syscall_dispatcher();
	}
}

void QuadraTranslator::begin_function(QuadraFunction function)
{
	_function = std::move(function);
	
	auto blocks = _function.ghidra->getBasicBlocks().getList();
	assert(blocks.size() >= 1);
	_builder.SetInsertPoint(get_block(blocks[0])->llvm);
	
	if(!STORE_REGISTERS_IN_GLOBAL) {
		auto registers_type = llvm::ArrayType::get(int_type(1), _register_space_size);
		llvm::AllocaInst* registers_alloca = _builder.CreateAlloca(registers_type, nullptr, "registers");
		_register_space = registers_alloca->getType()->getAddressSpace();
		auto register_ptr_type = llvm::PointerType::get(int_type(1), _register_space);
		_function.register_alloca = _builder.CreatePointerCast(registers_alloca, register_ptr_type, "");
	}
	
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
		case CPUI_LOAD: { // 2
			assert(isize == 2);
			type = llvm::PointerType::get(int_type(op.getOut()->getSize()), _function.stack_space);
			tmp1 = decompress_pointer(inputs[1], _function.stack_alloca, type);
			output = _builder.CreateLoad(tmp1, "");
			break;
		}
		case CPUI_STORE: // 3
			assert(isize == 3);
			type = llvm::PointerType::get(int_type(op.getIn(2)->getSize()), _function.stack_space);
			tmp1 = decompress_pointer(inputs[1], _function.stack_alloca, type);
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
			assert(isize == 1);
			FuncCallSpecs* call = _function.ghidra->getCallSpecs(&op);
			Address callee_addr = call->getEntryAddress();
			QuadraFunction* callee = get_function(callee_addr, nullptr);
			llvm::Value* return_value = _builder.CreateCall(callee->llvm, {}, "", nullptr);
			VarnodeData return_reg = _arch->translate->getRegister(_arch->return_register());
			Address return_addr = Address(_arch->getSpaceByName("register"), return_reg.offset);
			llvm::Value* v0 = get_local(_function.ghidra->newVarnode(
				return_reg.size, return_addr));
			output = _builder.CreateStore(return_value, v0);
			break;
		}
		case CPUI_CALLOTHER: // 9
			output = _builder.CreateCall(_syscall_dispatcher);
			return;
		case CPUI_RETURN: { // 10
			assert(isize == 1);
			// HACK!
			VarnodeData return_reg = _arch->translate->getRegister(_arch->return_register());
			Address return_addr = Address(_arch->getSpaceByName("register"), return_reg.offset);
			tmp1 = get_input(_function.ghidra->newVarnode(
				return_reg.size, return_addr));
			output = _builder.CreateRet(tmp1);
			block.emitted_branch = true;
			break;
		}
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
			assert(isize == 2);
			// Obviously wrong!
			output = _builder.CreateTrunc(inputs[0], int_type(op.getOut()->getSize()));
			break;
		case CPUI_INT_SCARRY: // 22
			assert(isize == 2);
			// Obviously wrong!
			output = _builder.CreateTrunc(inputs[0], int_type(op.getOut()->getSize()));
			break;
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
			tmp1 = _builder.CreateZExt(inputs[1], int_type(op.getIn(0)->getSize()), "");
			output = _builder.CreateShl(inputs[0], tmp1, "", false, false);
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
		}
		case CPUI_POPCOUNT: // 72
			assert(isize == 1);
			// Obviously wrong!
			output = _builder.CreateTrunc(inputs[0], int_type(op.getOut()->getSize()));
			break;
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
	// The Funcdata object is owned by the FunctionSymbol object, which is in
	// turn owned by a Scope object, which is owned by a Database object, which
	// is owned by the Architecture object.
	FunctionSymbol* symbol = _arch->symboltab->getGlobalScope()->addFunction(address, name_ss.str());
	function.ghidra = symbol->getFunction();
	
	// Generate pcode ops, basic blocks and call specs.
	function.ghidra->startProcessing();
	assert(!function.ghidra->hasBadData() && "Function flowed into bad data!!!");
	
	llvm::FunctionType* func_type = llvm::FunctionType::get(llvm::Type::getInt32Ty(_context), false);
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
	
	// Point the MIPS stack pointer at the stack allocation.
	if(var->getSpace()->getName() == "register" && var->getOffset() == 0x1d0) {
		return _builder.CreatePtrToInt(_function.stack_alloca, int_type(var->getSize()));
	}
	
	return _builder.CreateLoad(get_local(var), "");
}

llvm::Value* QuadraTranslator::get_local(const Varnode* var)
{
	if(var->getSpace()->getName() == "register") {
		return get_register(var->getOffset(), var->getSize());
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

llvm::Value* QuadraTranslator::get_register(uintb offset, int4 size_bytes)
{
	auto offset_const = llvm::ConstantInt::get(_context, llvm::APInt(32, offset, false));
	llvm::Value* ptr8 = _builder.CreateGEP(register_storage(), offset_const, "");
	auto ptr_type = llvm::PointerType::get(int_type(size_bytes), _register_space);
	return _builder.CreatePointerCast(ptr8, ptr_type, "");
}

llvm::Value* QuadraTranslator::decompress_pointer(llvm::Value* val, llvm::Value* hi, llvm::Type* ptr_type)
{
	auto lo_mask = llvm::ConstantInt::get(_context, llvm::APInt(64, 0x00000000ffffffff, false));
	auto hi_mask = llvm::ConstantInt::get(_context, llvm::APInt(64, 0xffffffff00000000, false));
	auto stack_ptr = _builder.CreatePtrToInt(hi, int_type(8));
	auto stack_ptr_hi = _builder.CreateAnd(stack_ptr, hi_mask);
	auto lo_ptr = _builder.CreateZExt(val, int_type(8));
	auto combined_ptr = _builder.CreateOr(stack_ptr_hi, lo_ptr);
	return _builder.CreateIntToPtr(combined_ptr, ptr_type);
}

llvm::Value* QuadraTranslator::register_storage()
{
	if(STORE_REGISTERS_IN_GLOBAL) {
		return _registers_global;
	} else {
		return _function.register_alloca;
	}
}

llvm::Value* QuadraTranslator::zero(int4 bytes)
{
	return llvm::ConstantInt::get(int_type(bytes), llvm::APInt(bytes * 8, 0, false));
}

llvm::Type* QuadraTranslator::int_type(int4 bytes)
{
	return llvm::IntegerType::get(_context, bytes * 8);
}

void QuadraTranslator::create_printf_int(const char* fmt, llvm::Value* val)
{
	// https://stackoverflow.com/questions/30234027/how-to-call-printf-in-llvm-through-the-module-builder-system
	llvm::Function* printf = _module.getFunction("printf");
	if(printf == nullptr) {
		llvm::PointerType *Pty = llvm::PointerType::get(llvm::IntegerType::get(_module.getContext(), 8), 0);
		llvm::FunctionType *FuncTy9 = llvm::FunctionType::get(llvm::IntegerType::get(_module.getContext(), 32), true);

		printf = llvm::Function::Create(FuncTy9, llvm::GlobalValue::ExternalLinkage, "printf", &_module);
		printf->setCallingConv(llvm::CallingConv::C);

		llvm::AttributeList printf_attr_list;
		printf->setAttributes(printf_attr_list);
	}
	
	llvm::Value* args[2] = { _builder.CreateGlobalStringPtr(fmt), val };
	llvm::ArrayRef<llvm::Value*> args_ref(args, args + 2);
	_builder.CreateCall(_module.getFunction("printf"), args_ref);
}

llvm::Function* QuadraTranslator::create_syscall_dispatcher()
{
	//DocumentStorage doc_store;
	//Document* languages_xml = doc_store.openDocument("syscalls/languages.xml");
	
	llvm::FunctionType* func_type = llvm::FunctionType::get(llvm::Type::getInt32Ty(_context), false);
	llvm::Function* dispatcher = llvm::Function::Create(
		func_type,
		llvm::Function::ExternalLinkage,
		"__quadra_dispatch_syscall",
		_module);
	
	llvm::BasicBlock* entry = llvm::BasicBlock::Create(_context, "entry", dispatcher);
	_builder.SetInsertPoint(entry);
	
	std::vector<std::string> arg_reg_names = _arch->syscall_argument_registers();
	std::vector<llvm::Value*> arg_regs;
	for(auto& name : arg_reg_names) {
		VarnodeData reg = _arch->translate->getRegister(name);
		arg_regs.push_back(get_register(reg.offset, reg.size));
	}
	
	struct SyscallInfo {
		std::string symbol;
		llvm::Type* return_type;
		std::vector<llvm::Type*> argument_types;
	};
	
	llvm::Type* char_type = llvm::Type::getInt8Ty(_context);
	llvm::Type* u32_type = llvm::Type::getInt32Ty(_context);
	llvm::Type* char_ptr_type = llvm::PointerType::get(char_type, _register_space);
	std::map<int, SyscallInfo> syscalls {
		{4001, {"qsys_exit", u32_type, {u32_type}}},
		{4003, {"qsys_read", u32_type, {u32_type, char_ptr_type, u32_type}}},
		{4004, {"qsys_write", u32_type, {u32_type, char_ptr_type, u32_type}}},
		{4005, {"qsys_open", u32_type, {char_ptr_type, u32_type, u32_type}}},
		{4006, {"qsys_close", u32_type, {u32_type}}}
	};
	
	// Used to get the stack pointer.
	llvm::AllocaInst* dummy_alloca = _builder.CreateAlloca(int_type(1), nullptr, "stackframe");
	
	VarnodeData syscall_number_reg = _arch->translate->getRegister(_arch->syscall_return_register());
	
	llvm::Value* v0_ptr = get_register(syscall_number_reg.offset, 4);
	auto syscall_number = _builder.CreateLoad(v0_ptr);
	
	for(auto& [number, syscall] : syscalls) {
		llvm::Value* number_val = llvm::ConstantInt::get(_context, llvm::APInt(32, number, false));
		auto cond = _builder.CreateICmpEQ(syscall_number, number_val, "cmp");
		
		llvm::BasicBlock* truthy = llvm::BasicBlock::Create(_context, "sys_" + std::to_string(number), dispatcher);
		llvm::BasicBlock* continuation = llvm::BasicBlock::Create(_context, "", dispatcher);
		_builder.CreateCondBr(cond, truthy, continuation);
		
		llvm::FunctionType* wrapper_type = llvm::FunctionType::get(syscall.return_type, syscall.argument_types, false);
		llvm::Function* wrapper = llvm::Function::Create(
			wrapper_type,
			llvm::GlobalValue::ExternalLinkage,
			syscall.symbol,
			_module);
		
		_builder.SetInsertPoint(truthy);
		std::vector<llvm::Value*> args;
		for(int i = 0; i < syscall.argument_types.size(); i++) {
			llvm::Value* arg = _builder.CreateLoad(arg_regs[i]);
			if(syscall.argument_types[i]->isPointerTy()) {
				arg = decompress_pointer(arg, dummy_alloca, syscall.argument_types[i]);
			} else {
				arg = _builder.CreateZExtOrTrunc(arg, syscall.argument_types[i]);
			}
			args.push_back(arg);
		}
		auto result = _builder.CreateCall(wrapper, args);
		_builder.CreateStore(result, v0_ptr, false);
		_builder.CreateBr(continuation);
		_builder.SetInsertPoint(continuation);
	}
	
	_builder.CreateRet(llvm::ConstantInt::get(_context, llvm::APInt(32, 0, false)));
	
	return dispatcher;
}
