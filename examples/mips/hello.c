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
		: "=r" (retval) : "r" (num), "r" (arg0), "r" (arg1), "r" (arg2));
	return retval;
}

// Stack strings.
#define SS(o, c1) o[i++] = c1;
#define SS4(o, c1, c2, c3, c4) SS(o, c1); SS(o, c2); SS(o, c3); SS(o, c4);
#define SS12(o, c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c11, c12) \
	i = 0; \
	SS4(o, c1, c2, c3, c4); \
	SS4(o, c5, c6, c7, c8); \
	SS4(o, c9, c10, c11, c12);

#define STDOUT
#define O_WRONLY	00000001

void _start()
{
	char path[12];
	int i;
	
	char greeting[12];
	SS12(greeting, 'H', 'e', 'l', 'l', 'o', ' ', 'w', 'o', 'r', 'l', 'd', '!');
	syscall(SYS_write, 1, (u32) greeting, 12);
}
