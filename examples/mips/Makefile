all: add mul loop function intargs
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
