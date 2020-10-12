all: add

add: add.asm
	nasm -f elf32 add.asm
	ld -m elf_i386 add.o -o add
