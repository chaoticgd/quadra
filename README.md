# quadra

Many-to-many binary translation using Ghidra and LLVM.

## Building

	cmake -DGHIDRA_DIR=<ghidra repo root> -DLLVM_DIR=<LLVM build dir>/lib/cmake/llvm/
	cmake --build .

## Running

	./quadra <input binary>

Quadra has been tested to work on Ubuntu Linux 20.04.

The `GHIDRA_DIR` enviroment variable must be set to the path of a Ghidra installation. The MIPS processor currently supported is the R5900, so the Ghidra installation must have the [ghidra-emotionengine](https://github.com/beardypig/ghidra-emotionengine) plugin installed.
