#ifndef _EPOLL_H_
#define _EPOLL_H_


/**
 * @brief 对epoll接口进行初步的封装
 * @
 * 
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <sys/queue.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>

#include "ev_log.h"
#include "ev_event.h"
#include "ev_event-internal.h"

/*
* 由于 epoll 接口的限制，我们需要跟踪
* 所有文件描述符。
*/
struct evepoll {
	struct event *evread;//read事件
	struct event *evwrite;//write事件
};


/// parma:fds  数组用于跟踪每个文件描述符上的读事件和写事件
/// param nfds  fds 数组的大小
/// param events  用于存储从 epoll_wait 返回的事件
/// param:nevents  events 数组的大小 每次调用epoll_wait时最多可以返回的事件数量
/// param:epfd  epoll 实例的文件描述符
struct epollop {
    struct evepoll *fds;       // 指向 evepoll 结构体数组的指针
    int nfds;                  // fds 数组的大小（文件描述符的数量）
    struct epoll_event *events; // 指向 epoll_event 结构体数组的指针
    int nevents;               // events 数组的大小（epoll 事件的数量）
    int epfd;                  // epoll 实例的文件描述符
};

/// @brief 对封装的epollop进行初始化(epfd events nevents nfds)
/// @param event_base 需要初始化的event_base实例
/// @return 成功返回初始化后的epollop实例，失败返回NULL
static void *epoll_init	(struct event_base *);
static int epoll_add	(void *, struct event *);
static int epoll_del	(void *, struct event *);
static int epoll_dispatch	(struct event_base *, void *, struct timeval *);
static void epoll_dealloc	(struct event_base *, void *);




extern const struct eventop epollops;


/* On Linux kernels at least up to 2.6.24.4, epoll can't handle timeout
 * values bigger than (LONG_MAX - 999ULL)/HZ.  HZ in the wild can be
 * as big as 1000, and LONG_MAX can be as small as (1<<31)-1, so the
 * largest number of msec we can support here is 2147482.  Let's
 * round that down by 47 seconds.
 */
#define MAX_EPOLL_TIMEOUT_MSEC (35*60*1000)

#define INITIAL_NFILES 32
#define INITIAL_NEVENTS 32
#define MAX_NEVENTS 4096

#ifdef __cplusplus
}
#endif

#endif /* _EPOLL_H_ */