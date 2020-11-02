all: add mul loop

add: add.S
	ee-gcc add.S -o add -nostdlib

mul: mul.S
	ee-gcc mul.S -o mul -nostdlib

loop: ../loop.c
	ee-gcc ../loop.c -o loop -nostdlib -O3
