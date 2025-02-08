#ifndef _EVENT_H_
#define _EVENT_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <sys/queue.h>
#include <stdio.h>
#include <stdlib.h>

struct event_base;

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

#ifdef __cplusplus
}
#endif

#endif /* _EVENT_H_ */