section .text
global _start
_start:
	xor rdi, rdi
	add rdi, 1
	add rdi, 2
	add rdi, 3
	add rdi, 4
	mov rax, 60 ; exit
	syscall
	ret
