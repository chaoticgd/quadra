all: add mul loop function intargs syscall hello
add: add.S
	ee-gcc add.S -o add -nostdlib
mul: mul.S
	ee-gcc mul.S -o mul -nostdlib
loop: ../loop.c
	ee-gcc ../loop.c -o loop -nostdlib -O3
function: ../function.c
	ee-gcc ../function.c -o function -nostdlib
intargs: ../intargs.c
	ee-gcc ../intargs.c -o intargs -nostdlib
syscall: syscall.c
	ee-gcc syscall.c -o syscall -nostdlib -O3
hello: hello.c
	ee-gcc hello.c -o hello -nostdlib -O3
