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
	# NOTE: This demo.sh file has been modified since it was used in the
	# presentation such that you don't need a MIPS toolchain installed in order
	# to run it.
	cat "$1"
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
	("add")     run_demo dis/mips_add.txt "" examples/mips/add;;
	("loop")    run_demo dis/mips_loop.txt examples/loop.c examples/mips/loop;;
	("intargs") run_demo dis/mips_intargs.txt examples/intargs.c examples/mips/intargs;;
	("hello")   run_demo dis/mips_hello.txt examples/mips/hello.c examples/mips/hello;;
	# x86 examples
	("x86add")  run_demo    dis/x86_add.txt "" examples/x86/add;;
esac
