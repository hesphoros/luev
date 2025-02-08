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
    unsigned int min_heap_index;
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

    // 记录了当前激活事件的类型
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
void event_base_free(struct event_base *);



void event_active(struct event *ev, int res, short ncalls);
#define _EVENT_LOG_DEBUG 0
#define _EVENT_LOG_MSG   1
#define _EVENT_LOG_WARN  2
#define _EVENT_LOG_ERR   3
typedef void (*event_log_cb)(int severity, const char *msg);

#ifdef __cplusplus
}
#endif

#endif /* _EVENT_H_ */