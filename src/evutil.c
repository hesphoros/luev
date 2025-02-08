#include "evutil.h"
#include "common.h"
#include "log.h"


static int evutil_issetugid(void){return (getuid() != geteuid());}


// 定义一个函数 evutil_getenv，用于获取环境变量的值
// 参数 varname 是一个指向常量字符的指针，表示环境变量的名称
// 返回值是一个指向常量字符的指针，表示环境变量的值，如果获取失败则返回 NULL
const char *
evutil_getenv(const char *varname)
{
    // 调用 evutil_issetugid 函数检查当前进程是否设置了 setuid 或 setgid 位
    // 如果设置了，则返回 NULL，表示不能在设置了 setuid 或 setgid 的情况下获取环境变量
	if (evutil_issetugid())
		return NULL;

    // 如果没有设置 setuid 或 setgid 位，则调用标准库函数 getenv 获取环境变量的值
    // getenv 函数的参数是环境变量的名称，返回值是指向环境变量值的指针
	return getenv(varname);
}


int evutil_socketpair(int family, int type, int protocol, int fd[2])
{
	return socketpair(family, type, protocol, fd);
}


int evutil_make_socket_nonblocking(int fd)
{
    int flags; // 用于存储文件描述符的当前标志
    // 获取文件描述符的当前标志
    if ((flags = fcntl(fd, F_GETFL, NULL)) < 0) {
        // 如果获取失败，输出警告信息
        event_warn("fcntl(%d, F_GETFL)", fd);
        return -1; // 返回-1表示失败
    }
    // 设置文件描述符的标志为非阻塞
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        // 如果设置失败，输出警告信息
        event_warn("fcntl(%d, F_SETFL)", fd);
        return -1; // 返回-1表示失败
    }

	return 0; // 返回0表示成功
}


int
evutil_snprintf(char *buf, size_t buflen, const char *format, ...)
{
	int r;
	va_list ap;
	va_start(ap, format);
	r = evutil_vsnprintf(buf, buflen, format, ap);
	va_end(ap);
	return r;
}


int
evutil_vsnprintf(char *buf, size_t buflen, const char *format, va_list ap)
{
	int r = vsnprintf(buf, buflen, format, ap);
	buf[buflen-1] = '\0';
	return r;
}