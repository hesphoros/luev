#ifndef _EVENT_INTERNAL_H_
#define _EVENT_INTERNAL_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include <sys/queue.h>
#include "event.h"
#include "evsignal.h"


struct eventop{
    const char *name;
	void *(*init)(struct event_base *);//初始化
	int (*add)(void *, struct event *);//添加事件
	int (*del)(void *, struct event *);//删除事件
	int (*dispatch)(struct event_base *, void *, struct timeval *);//分发事件
	void (*dealloc)(struct event_base *, void *);//释放资源
	/* set if we need to reinitialize the event base */
	int need_reinit;
};


struct min_heap{int summy;};


struct event_base{
    //事件处理后端select epoll poll
    const struct eventop * evsel;
    void*evbase;
    int event_count;
    int event_count_active;

    /* Set to terminate loop */
    int event_gotterm;
    /* Set to terminate loop immediately*/
    int event_break;
    /* active event management */
	struct event_llist **activequeues;
    int nactivequeues;

    /*Signal handing info  */
    struct evsignal_info sig;
    //链表，保存了所有的注册事件 event 的指针。
    struct event_list eventqueue;
	struct timeval event_tv;

    //管理定时事件的最小堆
	struct min_heap timeheap;

	struct timeval tv_cache;

};


int evsignal_init(struct event_base *);
void evsignal_process(struct event_base *);
int evsignal_add(struct event *);
int evsignal_del(struct event *);
void evsignal_dealloc(struct event_base *);

#ifdef __cplusplus
}
#endif

#endif /* _EVENT_INTERNAL_H_ */