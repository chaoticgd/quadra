all: add loop
add: add.asm
	nasm -f elf64 add.asm
	ld add.o -o add
loop: ../loop.c
	gcc ../loop.c -o loop -nostdlib
