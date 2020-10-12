#include <regdef.h>
.global _start
_start:
	lui a1, 0
	addi a1, a1, 1
	addi a1, a1, 2
	addi a1, a1, 3
	addi a1, a1, 4
	lui a0, 0
	ori a0, a0, 4001
	syscall // exit
