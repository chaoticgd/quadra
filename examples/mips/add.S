#include <regdef.h>
.global _start
_start:
	lui v0, 0
	addi v0, v0, 1
	addi v0, v0, 2
	addi v0, v0, 3
	addi v0, v0, 4
	jr ra
