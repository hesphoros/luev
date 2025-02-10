#ifndef _EVENT_H_
#define _EVENT_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <sys/queue.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>

#define EVLIST_TIMEOUT	0x01
#define EVLIST_INSERTED	0x02
#define EVLIST_SIGNAL	0x04
#define EVLIST_ACTIVE	0x08
#define EVLIST_INTERNAL	0x10
#define EVLIST_INIT	0x80

/* EVLIST_X_ Private space: 0x1000-0xf000 */
#define EVLIST_ALL	(0xf000 | 0x9f)

#define EV_TIMEOUT	0x01
#define EV_READ		0x02
#define EV_WRITE	0x04
#define EV_SIGNAL	0x08
#define EV_PERSIST	0x10	/* Persistant event */


struct event_base;
TAILQ_HEAD (event_list, event);
struct event {
    // 已注册事件链表
    TAILQ_ENTRY (event) ev_next;
    // 激活事件链表
	TAILQ_ENTRY (event) ev_active_next;
    // 信号事件链表
	TAILQ_ENTRY (event) ev_signal_next;

    //最小时间堆索引
    unsigned int min_heap_idx;
    //事件基
    struct event_base *ev_base;
    //文件描述符 or 信号
    int ev_fd;

    //事件类型 超时 读写 信号 永久
    short ev_events;
    // 事件就绪执行时，调用 ev_callback 的次数 通常为1
	short ev_ncalls;
	// 指向 ev_ncalls 的指针，用于在回调函数中删除事件
	short *ev_pncalls;

	struct timeval ev_timeout;

	int ev_pri;		/* smaller numbers are higher priority */

	/// @brief 回调函数
    /// @param fd 文件描述符 or 信号
    /// @param arg 回调函数参数
	void (*ev_callback)(int, short, void *arg);
	void *ev_arg;

    //回调函数中传递的结果
	int ev_res;		/* result passed to event callback */

    //标记 event 信息的字段
    /* 可能取值
    * EVLIST_TIMEOUT    // event在time堆中
    * EVLIST_INSERTED   // event在已注册事件链表中
    * EVLIST_SIGNAL     // event在信号事件链表中
    * EVLIST_ACTIVE     // event在激活链表中
    * EVLIST_INTERNAL   // 内部使用标记
    * EVLIST_INIT       // event初始化标记
    */
	int ev_flags;

};


/**
  Initialize the event API.

  使用 event_base_new() 初始化新的事件库，但不设置 current_base 全局变量。
  如果仅使用 event_base_new()，则添加的每个事件都必须使用 event_base_set() 设置事件库。
  @see event_base_set(), event_base_free(), event_init()
 */
struct event_base *event_base_new(void);


/**
  Initialize the event API.

  事件 API 需要先使用 event_init() 进行初始化，然后才能使用。
  设置 current_base 全局变量，表示没有关联基础的事件的默认基础。

  @see event_base_set(), event_base_new()
 */
struct event_base *event_init(void);


/**
    解除与 event_base 关联的所有内存分配，并释放该基址。

    请注意，此函数不会关闭任何 fds 或释放作为回调参数传递给 event_set 的任何内存。
  @param eb an event_base to be freed
 */
//TODO: 待完善
void event_base_free(struct event_base *);


int event_base_priority_init(struct event_base *base, int npriorities);

/// @brief 设置事件ev绑定的fd or signal,对于定时事件设为-1即可
/// @param ev 需要设置的event
/// @param fd file discriptor or signal
/// @param events 事件类型（读、写、等）EV_READ|EV_PERSIST, EV_WRITE, EV_SIGNAL
/// @param callback 回调函数
/// @param arg 函调函数参数
void event_set(struct event *ev, int fd, short events,void (*callback)(int, short, void *), void *arg);




/**
 将不同的事件库与某个事件关联。

  @param eb the event base
  @param ev the event
 */
int event_base_set(struct event_base *, struct event *);

/**


void event_active(struct event *ev, int res, short ncalls);

#define _EVENT_LOG_DEBUG 0
#define _EVENT_LOG_MSG   1
#define _EVENT_LOG_WARN  2
#define _EVENT_LOG_ERR   3
typedef void (*event_log_cb)(int severity, const char *msg);


#define EVENT_SIGNAL(ev)	(int)(ev)->ev_fd
#define EVENT_FD(ev)		(int)(ev)->ev_fd


/**
 将事件添加到受监控事件集。

    函数 event_add() 会在 event_set() 中指定的事件发生时或至少在 tv 中
    指定的时间内安排执行 ev 事件。如果 tv 为 NULL，则不会发生超时，
    并且只有当文件描述符上发生匹配事件时才会调用该函数。
    ev 参数中的事件必须已由 event_set() 初始化，
    并且不能在对 event_set() 的调用中使用，直到它超时或被 event_del()
    删除如果 ev 参数中的事件已经安排了超时，则旧的超时将被新的超时替换。

  @param ev an event struct initialized via event_set()
  @param timeout the maximum amount of time to wait for the event, or NULL
         to wait forever
  @return 0 if successful, or -1 if an error occurred
  @see event_del(), event_set()
  */
 int event_add(struct event *ev, const struct timeval *timeout);




/**
    从受监控事件集中删除一个事件。

    函数 event_del() 将取消参数 ev 中的事件。如果
    事件已执行或从未添加，则调用将不会产生
    效果。

  @param ev an event struct to be removed from the working set
  @return 0 if successful, or -1 if an error occurred
  @see event_add()
 */
int event_del(struct event *);

/**
  Add a timer event.

  @param ev the event struct
  @param tv timeval struct
 */
#define evtimer_add(ev, tv)		event_add(ev, tv)

#ifdef __cplusplus
}
#endif

#endif /* _EVENT_H_ */