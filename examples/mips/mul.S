#include <regdef.h>
.global _start
_start:
	lui a0, 0
	addi a0, a0, 5
	addi a1, a0, 6
	mult a0, a1
	mflo v0
	jr ra
