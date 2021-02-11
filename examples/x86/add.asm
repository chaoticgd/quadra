section .text
global _start
_start:
	xor rax, rax
	add rax, 1
	add rax, 2
	add rax, 3
	add rax, 4
	ret
