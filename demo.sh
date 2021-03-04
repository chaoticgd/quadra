heading() {
	echo ================================================================================
	echo $1
	echo ================================================================================
}

run_demo() {
	if [[ "$2" != "" ]]; then # Skip printing out the source code for hand assembly.
		heading "Source code"
		cat "$2"
	fi
	heading "Disassembly"
	"$1" -D "$3"
	heading "P-Code IR"
	./quadra "$3" > /tmp/prog.ll
	heading "Compiling and running"
	clang++ /tmp/prog.ll libmips_o32_linux.a -o /tmp/prog
	/tmp/prog
	echo "exit code: $?"
}

# A number of pre-prepared example programs.
case "$1" in
	# MIPS examples
	("add")     run_demo ee-objdump "" examples/mips/add;;
	("addc")    run_demo ee-objdump examples/addc.c examples/mips/addc;;
	("loop")    run_demo ee-objdump examples/loop.c examples/mips/loop;;
	("intargs") run_demo ee-objdump examples/intargs.c examples/mips/intargs;;
	("hello")   run_demo ee-objdump examples/mips/hello.c examples/mips/hello;;
	# x86 examples
	("x86add")  run_demo    objdump "" examples/x86/add;;
esac
