section .text
global _start
_start:
	xor ebx, ebx
	add ebx, 1
	add ebx, 2
	add ebx, 3
	add ebx, 4
	mov eax, 1
	int 0x80 ; exit
