#include <unistd.h>
#include <linux/types.h>

#define TRACE(...) //__VA_ARGS__

unsigned int qsys_exit(unsigned int error_code)
{
	TRACE(printf("exit(%d)\n", error_code));
	exit(error_code);
}

unsigned int qsys_read(unsigned int fd, char* buf, size_t count)
{
	TRACE(printf("read(%d, \"%s\", %ld)\n", fd, buf, count));
	read(fd, buf, count);
}

unsigned int qsys_write(unsigned int fd, const char* buf, size_t count)
{
	TRACE(printf("write(%d, \"%s\", %ld)\n", fd, buf, count));
	write(fd, buf, count);
}

unsigned int qsys_open(const char* filename, int flags, unsigned short mode)
{
	TRACE(printf("open(\"%s\", %d, %d)\n", filename, flags, mode));
	open(filename, flags, mode);
}

unsigned int qsys_close(unsigned int fd)
{
	TRACE(printf("close(%d)\n", fd));
	close(fd);
}
