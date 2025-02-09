#ifndef _EVUTIL_H_
#define _EVUTIL_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <sys/types.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdarg.h>

#define ev_uint64_t uint64_t
#define ev_int64_t int64_t
#define ev_uint32_t uint32_t
#define ev_int32_t int32_t
#define ev_uint16_t uint16_t
#define ev_int16_t int16_t
#define ev_uint8_t uint8_t
#define ev_int8_t int8_t



/**
 * @brief 获取环境变量的值
 *
 * 该函数用于从当前进程的环境变量中查找指定名称的环境变量，并返回其值。
 * 如果环境变量不存在，则返回 NULL。
 *
 * @param varname 要查找的环境变量名称（以 null 结尾的字符串）。
 * @return 返回环境变量的值（以 null 结尾的字符串）。如果环境变量不存在，则返回 NULL。
 *
 * @note 该函数是对标准库函数 `getenv` 的封装
 *
 * @example
 *   const char *home = evutil_getenv("HOME");
 *   if (home) {
 *       printf("Home directory: %s\n", home);
 *   } else {
 *       printf("HOME environment variable not found.\n");
 *   }
 */
const char *evutil_getenv(const char *varname);

/// @brief 创建socketpair
/// @param family AF_INET, AF_INET6, AF_UNIX
/// @param type SOCK_STREAM, SOCK_DGRAM, SOCK_RAW, SOCK_SEQPACKET
/// @param protocol IPPROTO_TCP, IPPROTO_UDP, IPPROTO_SCTP
/// @param fd 一个长度为2的数组，用于存储
/// @return
int evutil_socketpair(int family, int type, int protocol, int fd[2]);


/// @brief 设置socket为非阻塞
/// @param fd socket文件描述符
/// @return
int evutil_make_socket_nonblocking(int fd);

#define EVUTIL_CLOSESOCKET(s) close(s)
#define EVUTIL_SOCKET_ERROR() (errno)
#define EVUTIL_SET_SOCKET_ERROR(errcode)		\
		do { errno = (errcode); } while (0)

/**timeradd, timersub, timerclear */
#define evutil_timeradd(tvp, uvp, vvp) timeradd((tvp), (uvp), (vvp))
#define evutil_timersub(tvp, uvp, vvp) timersub((tvp), (uvp), (vvp))
#define evutil_timerclear(tvp)          timerclear(tvp)
#define	evutil_timercmp(tvp, uvp, cmp)							\
	(((tvp)->tv_sec == (uvp)->tv_sec) ?							\
	 ((tvp)->tv_usec cmp (uvp)->tv_usec) :						\
	 ((tvp)->tv_sec cmp (uvp)->tv_sec))

#define evutil_timerisset(tvp) timerisset(tvp)
#define evutil_gettimeofday(tv, tz) gettimeofday((tv), (tz))


int evutil_snprintf(char *buf, size_t buflen, const char *format, ...)
#ifdef __GNUC__
	__attribute__((format(printf, 3, 4)))
#endif
	;
int evutil_vsnprintf(char *buf, size_t buflen, const char *format, va_list ap);

#define UNUSED_PARAM(x) (void)(x)

#ifdef __cplusplus
}
#endif
#endif /* _EVUTIL_H_ */