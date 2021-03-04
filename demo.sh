heading() {
	echo ================================================================================
	echo $1
	echo ================================================================================
}

run_demo() {
	heading "Source code"
	cat "$1"
	heading "MIPS Disassembly"
	ee-objdump -D "$2"
	heading "P-Code IR"
	./quadra "$2" > /tmp/prog.ll
	heading "Compiling and running"
	clang++ /tmp/prog.ll libmips_o32_linux.a -o /tmp/prog
	/tmp/prog
	echo "exit code: $?"
}

# A number of pre-prepared example programs.
case "$1" in
	("add") run_demo examples/addc.c examples/mips/addc;;
	("loop") run_demo examples/loop.c examples/mips/loop;;
	("intargs") run_demo examples/intargs.c examples/mips/intargs;;
esac
