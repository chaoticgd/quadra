# quadra

Many-to-many binary translation using Ghidra and LLVM.

## Building

    cmake -DGHIDRA_DIR=<ghidra repo root> -DLLVM_DIR=<LLVM build dir>/lib/cmake/llvm/
	cmake --build .
