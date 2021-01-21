#include <unistd.h>
#include <linux/types.h>

unsigned int qsys_exit(unsigned int error_code)
{
	exit(error_code);
}

unsigned int qsys_read(unsigned int fd, char* buf, size_t count)
{
	read(fd, buf, count);
}

unsigned int qsys_write(unsigned int fd, const char* buf, size_t count)
{
	write(fd, buf, count);
}

unsigned int qsys_open(const char* filename, int flags, unsigned short mode)
{
	open(filename, flags, mode);
}

unsigned int qsys_close(unsigned int fd)
{
	close(fd);
}
