#include "event.h"
#include "event-internal.h"
#include <unistd.h>
#include "signal.h"
#include "epoll.h"
#include <stdio.h>
#include <time.h>

static const struct eventop * eventops[] = {
	&epollops,
// &pollops,
// &selectops
};

/* Global state */
struct event_base *current_base = NULL;
extern struct event_base *evsignal_base;
static int use_monotonic;



/* Handle signals - This is a deprecated interface */

/* 当 gotsig 设置时发出信号回调 */
int (*event_sigcb)(void);		/* Signal callback when gotsig is set */
volatile sig_atomic_t event_gotsig;	/* Set in signal handler */


/** Function defineations Area */
static void detect_monotonic(void);
/* Prototypes */
static void	event_queue_insert(struct event_base *, struct event *, int);
// 定义一个函数 event_init，用于初始化事件基础结构
// 函数返回类型为 struct event_base*，即指向 event_base 结构体的指针
struct event_base *event_init(void){

}



// 定义一个函数，用于创建并初始化一个新的event_base结构体
struct event_base *event_base_new(void){
    int i; // 用于循环计数
	struct event_base *base; // 指向event_base结构体的指针

	// 使用calloc函数分配内存，初始化为0，大小为struct event_base的大小
	// 如果分配失败，调用event_err函数输出错误信息并退出
	if ((base = calloc(1, sizeof(struct event_base))) == NULL)
		event_err(1, "%s: calloc", __func__);

	// 初始化全局信号回调函数指针为NULL
	event_sigcb = NULL;
	// 初始化全局信号接收标志为0
	event_gotsig = 0;

	// 检测系统是否支持单调递增的时钟
	detect_monotonic();
	// 获取当前时间，并存储到base->event_tv中
	gettime(base, &base->event_tv);

	// 初始化最小堆，用于管理时间事件
	min_heap_ctor(&base->timeheap);
	// 初始化事件队列，使用TAILQ（双向链表）结构
	TAILQ_INIT(&base->eventqueue);
	// 初始化信号管道，用于信号处理
	base->sig.ev_signal_pair[0] = -1;
	base->sig.ev_signal_pair[1] = -1;

	// 初始化事件机制选择器为NULL
	base->evbase = NULL;
	// 遍历所有可用的事件操作集，尝试初始化一个可用的事件机制
	for (i = 0; eventops[i] && !base->evbase; i++) {
		// 设置当前事件操作集
		base->evsel = eventops[i];

		// 调用当前事件操作集的初始化函数，如果成功则设置base->evbase
		base->evbase = base->evsel->init(base);
	}

	// 如果没有找到可用的事件机制，输出错误信息并退出
	if (base->evbase == NULL)
		event_errx(1, "%s: no event mechanism available", __func__);

	// 如果环境变量EVENT_SHOW_METHOD被设置，输出当前使用的事件机制名称
	if (evutil_getenv("EVENT_SHOW_METHOD"))
		event_msgx("libevent using: %s\n",
			   base->evsel->name);

	/* allocate a single active event queue */
	event_base_priority_init(base, 1);

	return (base);
}



static void
detect_monotonic(void)
{

	struct timespec ts;

	if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0)
		use_monotonic = 1;

}



void
event_active(struct event *ev, int res, short ncalls)
{
	/* We get different kinds of events, add them together */
	if (ev->ev_flags & EVLIST_ACTIVE) {
		ev->ev_res |= res;
		return;
	}

	ev->ev_res = res;
	ev->ev_ncalls = ncalls;
	ev->ev_pncalls = NULL;
	event_queue_insert(ev->ev_base, ev, EVLIST_ACTIVE);
}


void
event_queue_insert(struct event_base *base, struct event *ev, int queue)
{
	if (ev->ev_flags & queue) {
		/* Double insertion is possible for active events */
		if (queue & EVLIST_ACTIVE)
			return;

		event_errx(1, "%s: %p(fd %d) already on queue %x", __func__,
			   ev, ev->ev_fd, queue);
	}

	if (~ev->ev_flags & EVLIST_INTERNAL)
		base->event_count++;

	ev->ev_flags |= queue;
	switch (queue) {
	case EVLIST_INSERTED:
		TAILQ_INSERT_TAIL(&base->eventqueue, ev, ev_next);
		break;
	case EVLIST_ACTIVE:
		base->event_count_active++;
		TAILQ_INSERT_TAIL(base->activequeues[ev->ev_pri],
		    ev,ev_active_next);
		break;
	case EVLIST_TIMEOUT: {
		min_heap_push(&base->timeheap, ev);
		break;
	}
	default:
		event_errx(1, "%s: unknown queue %x", __func__, queue);
	}
}
