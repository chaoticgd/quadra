#ifndef _QUADRA_SYSCALL_DISPATCHER_H
#define _QUADRA_SYSCALL_DISPATCHER_H

#include <string>

#include <decompile/cpp/xml.hh>
#include <decompile/cpp/architecture.hh>

#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>

// This generates an LLVM function that compares the syscall number
llvm::Function* build_syscall_dispatcher(
	Architecture& arch,
	llvm::LLVMContext& ctx,
	llvm::Module& module,
	llvm::Value* registers,
	unsigned int register_space);

#endif
