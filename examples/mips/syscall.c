typedef unsigned int u32;

// Linux MIPS o32 ABI.
#define SYS_exit  4001
#define SYS_read  4003
#define SYS_write 4004
#define SYS_open  4005
#define SYS_close 4006

u32 syscall(u32 num, u32 arg0, u32 arg1, u32 arg2)
{
	u32 retval;
	asm volatile (
		"move $2, %1\n" // move v0, num
		"move $4, %2\n" // move a0, arg0
		"move $5, %3\n" // move a1, arg1
		"move $6, %4\n" // move a2, arg2
		"syscall\n"
		"move %0, $2\n" // move retval, v0
		: "=r" (retval)
		: "r" (num),
		  "r" (arg0),
		  "r" (arg1),
		  "r" (arg2));
	return retval;
}

int _start()
{
	syscall(SYS_exit, 123, 0, 0);
	return 10;
}
