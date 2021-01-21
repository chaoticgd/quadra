#include "syscall_dispatcher.h"

#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Verifier.h> // llvm::outs

llvm::Value* get_reg(
	llvm::LLVMContext& ctx,
	llvm::IRBuilder<>& builder,
	llvm::Value* registers,
	unsigned int register_space,
	uintb offset)
{
	auto offset_val = llvm::ConstantInt::get(ctx, llvm::APInt(32, offset, false));
	llvm::Value* ptr8 = builder.CreateGEP(registers, offset_val, "");
	auto ptr_type = llvm::PointerType::get(llvm::PointerType::getInt32Ty(ctx), register_space);
	return builder.CreatePointerCast(ptr8, ptr_type, "");
}

llvm::Function* build_syscall_dispatcher(
	Architecture& arch,
	llvm::LLVMContext& ctx,
	llvm::Module& module,
	llvm::Value* registers,
	unsigned int register_space)
{
	//DocumentStorage doc_store;
	//Document* languages_xml = doc_store.openDocument("syscalls/languages.xml");
	
	llvm::FunctionType* func_type = llvm::FunctionType::get(llvm::Type::getInt32Ty(ctx), false);
	llvm::Function* dispatcher = llvm::Function::Create(
		func_type,
		llvm::Function::ExternalLinkage,
		"__quadra_dispatch_syscall",
		module);
	
	llvm::BasicBlock* entry = llvm::BasicBlock::Create(ctx, "entry", dispatcher);
	
	llvm::IRBuilder<> builder(
		&dispatcher->getEntryBlock(),
		dispatcher->getEntryBlock().begin());
	
	std::vector<std::string> arg_reg_names = { "a0", "a1", "a2", "a3" };
	std::vector<llvm::Value*> arg_regs;
	for(auto& name : arg_reg_names) {
		uintb offset = arch.translate->getRegister(name).offset;
		arg_regs.push_back(get_reg(ctx, builder, registers, register_space, offset));
	}
	
	struct SyscallInfo {
		std::string symbol;
		int arg_count;
	};
	
	std::map<int, SyscallInfo> syscalls {
		{4001, {"qsys_exit", 1}},
		{4003, {"qsys_read", 3}},
		{4004, {"qsys_write", 3}},
		{4005, {"qsys_open", 3}},
		{4006, {"qsys_close", 1}}
	};
	
	uintb syscall_number_offset = arch.translate->getRegister("v0").offset;
	llvm::Value* v0_ptr = get_reg(ctx, builder, registers, register_space, syscall_number_offset);
	auto syscall_number = builder.CreateLoad(v0_ptr);
	
	for(auto& [number, syscall] : syscalls) {
		llvm::Value* number_val = llvm::ConstantInt::get(ctx, llvm::APInt(32, number, false));
		auto cond = builder.CreateICmpEQ(syscall_number, number_val, "cmp");
		
		llvm::BasicBlock* truthy = llvm::BasicBlock::Create(ctx, "sys_" + std::to_string(number), dispatcher);
		llvm::BasicBlock* continuation = llvm::BasicBlock::Create(ctx, "", dispatcher);
		builder.CreateCondBr(cond, truthy, continuation);
		
		std::vector<llvm::Type*> wrapper_args;
		for(int i = 0; i < syscall.arg_count; i++) {
			wrapper_args.push_back(llvm::Type::getInt32Ty(ctx));
		}
		llvm::FunctionType* wrapper_type = llvm::FunctionType::get(llvm::Type::getInt32Ty(ctx), wrapper_args, false);
		
		llvm::Function* wrapper = llvm::Function::Create(
			wrapper_type,
			llvm::GlobalValue::ExternalLinkage,
			syscall.symbol,
			module);
		
		builder.SetInsertPoint(truthy);
		std::vector<llvm::Value*> args;
		for(int i =0; i < syscall.arg_count; i++) {
			args.push_back(builder.CreateLoad(arg_regs[i]));
		}
		auto result = builder.CreateCall(wrapper, args);
		builder.CreateStore(result, v0_ptr, false);
		builder.CreateBr(continuation);
		builder.SetInsertPoint(continuation);
	}
	
	builder.CreateRet(llvm::ConstantInt::get(ctx, llvm::APInt(32, 0, false)));
	
	return dispatcher;
}
