#include "env_config.h"
#include "ev_event.h"
#include "ev_event-internal.h"
#include <unistd.h>
#include "signal.h"
#include <stdio.h>
#include <time.h>
#include "ev_min_heap.h"
#include <assert.h>
#include "ev_log.h"

// extern const struct eventop selectops;
extern const struct eventop pollops;
extern const struct eventop epollops;


static const struct eventop * eventops[] = {
	&epollops,
	&pollops,
// &selectops
};

/* Global state */
//全局event_base 指针
struct event_base *current_base = NULL;
extern struct event_base *evsignal_base;
/** 控制是否使用CLOCK_MONOTONIC 作为时间源*/
static int use_monotonic;
//TODO:
/**Prototypes */
static void	event_queue_insert(struct event_base *, struct event *, int);
static void	event_queue_remove(struct event_base *, struct event *, int);
static int	event_haveevents(struct event_base *);
static void timeout_correct(struct event_base *base, struct timeval *tv);

static int timeout_next(struct event_base *base, struct timeval **tv_p);
/* Handle signals - This is a deprecated interface */

/* 当 gotsig 设置时发出信号回调 */
int (*event_sigcb)(void);		/* Signal callback when gotsig is set */
volatile sig_atomic_t event_gotsig;	/* Set in signal handler */


/** Function defineations Area */
static void detect_monotonic(void);
static int gettime(struct event_base *base, struct timeval *tp);

// 定义一个函数 event_init，用于初始化事件基础结构
// 函数返回类型为 struct event_base*，即指向 event_base 结构体的指针
struct event_base *event_init(void){
	struct event_base *base = event_base_new();

	if (base != NULL)
		current_base = base;

	return (base);
}



static int
gettime(struct event_base *base, struct timeval *tp)
{
	if (base->tv_cache.tv_sec) {
		*tp = base->tv_cache;
		return (0);
	}


	if (use_monotonic) {
		struct timespec	ts;

		if (clock_gettime(CLOCK_MONOTONIC, &ts) == -1)
			return (-1);

		tp->tv_sec = ts.tv_sec;
		tp->tv_usec = ts.tv_nsec / 1000;
		return (0);
	}


	return (evutil_gettimeofday(tp, NULL));
}




int
event_reinit(struct event_base *base)
{
	const struct eventop *evsel = base->evsel;
	void *evbase = base->evbase;
	int res = 0;
	struct event *ev;

	/* check if this event mechanism requires reinit */
	if (!evsel->need_reinit)
		return (0);

	/* prevent internal delete */
	if (base->sig.ev_signal_added) {
		/* we cannot call event_del here because the base has
		 * not been reinitialized yet. */
		event_queue_remove(base, &base->sig.ev_signal,
		    EVLIST_INSERTED);
		if (base->sig.ev_signal.ev_flags & EVLIST_ACTIVE)
			event_queue_remove(base, &base->sig.ev_signal,
			    EVLIST_ACTIVE);
		base->sig.ev_signal_added = 0;
	}

	if (base->evsel->dealloc != NULL)
		base->evsel->dealloc(base, base->evbase);
	evbase = base->evbase = evsel->init(base);
	if (base->evbase == NULL)
		event_errx(1, "%s: could not reinitialize event mechanism",
		    __func__);

	TAILQ_FOREACH(ev, &base->eventqueue, ev_next) {
		if (evsel->add(evbase, ev) == -1)
			res = -1;
	}

	return (res);
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
		event_msgx("libevent using: %s",
			   base->evsel->name);

	/* allocate a single active event queue */
	event_base_priority_init(base, 1);

	return (base);
}



static void
detect_monotonic(void)
{
	struct timespec ts;

	if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0){
		use_monotonic = 1;
		event_msgx("Using monotonic clock for gettimeofday ");
		// printf("Using monotonic clock for gettimeofday\n");
	}
}



void
event_active(struct event *ev, int res, short ncalls)
{
	/* We get different kinds of events, add them together */
	// 检查事件是否已经在活动列表中
	if (ev->ev_flags & EVLIST_ACTIVE) {
		// 如果已经在活动列表中，将新的事件结果与现有结果进行按位或操作
		ev->ev_res |= res;
		// 直接返回，不需要再次插入活动队列
		return;
	}


	ev->ev_res = res;

	ev->ev_ncalls = ncalls;

	ev->ev_pncalls = NULL;
	// 将事件插入到事件基础结构的活动队列中，并标记为活动状态
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
			   (void* )ev, ev->ev_fd, queue);
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



void
event_queue_remove(struct event_base *base, struct event *ev, int queue)
{
	if (!(ev->ev_flags & queue))
		event_errx(1, "%s: %p(fd %d) not on queue %x", __func__,
			   (void*)ev, ev->ev_fd, queue);

	if (~ev->ev_flags & EVLIST_INTERNAL)
		base->event_count--;

	ev->ev_flags &= ~queue;
	switch (queue) {
	case EVLIST_INSERTED:
		TAILQ_REMOVE(&base->eventqueue, ev, ev_next);
		break;
	case EVLIST_ACTIVE:
		base->event_count_active--;
		TAILQ_REMOVE(base->activequeues[ev->ev_pri],
		    ev, ev_active_next);
		break;
	case EVLIST_TIMEOUT:
		min_heap_erase(&base->timeheap, ev);
		break;
	default:
		event_errx(1, "%s: unknown queue %x", __func__, queue);
	}
}




int
event_base_priority_init(struct event_base *base, int npriorities)
{
	int i;

	if (base->event_count_active)
		return (-1);

	if (npriorities == base->nactivequeues)
		return (0);

	if (base->nactivequeues) {
		for (i = 0; i < base->nactivequeues; ++i) {
			free(base->activequeues[i]);
		}
		free(base->activequeues);
	}

	/* Allocate our priority queues */
	base->nactivequeues = npriorities;
	base->activequeues = (struct event_list **)
	    calloc(base->nactivequeues, sizeof(struct event_list *));
	if (base->activequeues == NULL)
		event_err(1, "%s: calloc", __func__);

	for (i = 0; i < base->nactivequeues; ++i) {
		base->activequeues[i] = malloc(sizeof(struct event_list));
		if (base->activequeues[i] == NULL)
			event_err(1, "%s: malloc", __func__);
		TAILQ_INIT(base->activequeues[i]);
	}

	return (0);
}





void
event_set(struct event *ev, int fd, short events,
	  void (*callback)(int, short, void *), void *arg)
{
	/* Take the current base - caller needs to set the real base later */
	ev->ev_base = current_base;

	ev->ev_callback = callback;
	ev->ev_arg = arg;
	ev->ev_fd = fd;
	ev->ev_events = events;
	ev->ev_res = 0;
	ev->ev_flags = EVLIST_INIT;
	ev->ev_ncalls = 0;
	ev->ev_pncalls = NULL;

	min_heap_elem_init(ev);

	/* by default, we put new events into the middle priority */
	if(current_base)
		ev->ev_pri = current_base->nactivequeues/2;
}



int
event_haveevents(struct event_base *base)
{
	return (base->event_count > 0);
}


/**
 * 活动事件存储在优先级队列中。优先级较低的事件总是先于优先级较高的事件处理。
 * 优先级较低的事件可能会使优先级较高的事件"挨饿"。
*/
static void
event_process_active(struct event_base *base)
{
	struct event *ev;
	struct event_list *activeq = NULL;
	int i;
	short ncalls;

	for (i = 0; i < base->nactivequeues; ++i) {
		if (TAILQ_FIRST(base->activequeues[i]) != NULL) {
			activeq = base->activequeues[i];
			break;
		}
	}

	assert(activeq != NULL);

	for (ev = TAILQ_FIRST(activeq); ev; ev = TAILQ_FIRST(activeq)) {
		if (ev->ev_events & EV_PERSIST)
			event_queue_remove(base, ev, EVLIST_ACTIVE);
		else
			event_del(ev);

		/* Allows deletes to work */
		ncalls = ev->ev_ncalls;
		ev->ev_pncalls = &ncalls;
		while (ncalls) {
			ncalls--;
			ev->ev_ncalls = ncalls;
			(*ev->ev_callback)((int)ev->ev_fd, ev->ev_res, ev->ev_arg);
			if (base->event_break)
				return;
		}
	}
}



int
event_add(struct event *ev, const struct timeval *tv)
{
	struct event_base *base = ev->ev_base;
	const struct eventop *evsel = base->evsel;
	void *evbase = base->evbase;
	int res = 0;

	event_debug((
		 "event_add: event: %p, %s%s%scall %p",
		 ev,
		 ev->ev_events & EV_READ ? "EV_READ " : " ",
		 ev->ev_events & EV_WRITE ? "EV_WRITE " : " ",
		 tv ? "EV_TIMEOUT " : " ",
		 ev->ev_callback));

	assert(!(ev->ev_flags & ~EVLIST_ALL));

	/*
	 * prepare for timeout insertion further below, if we get a
	 * failure on any step, we should not change any state.
	 */
	if (tv != NULL && !(ev->ev_flags & EVLIST_TIMEOUT)) {
		if (min_heap_reserve(&base->timeheap,
			1 + min_heap_size(&base->timeheap)) == -1)
			return (-1);  /* ENOMEM == errno */
	}

	if ((ev->ev_events & (EV_READ|EV_WRITE|EV_SIGNAL)) &&
	    !(ev->ev_flags & (EVLIST_INSERTED|EVLIST_ACTIVE))) {
		res = evsel->add(evbase, ev);
		if (res != -1)
			event_queue_insert(base, ev, EVLIST_INSERTED);
	}

	/*
	 * we should change the timout state only if the previous event
	 * addition succeeded.
	 */
	if (res != -1 && tv != NULL) {
		struct timeval now;

		/* 
		 * we already reserved memory above for the case where we
		 * are not replacing an exisiting timeout.
		 */
		if (ev->ev_flags & EVLIST_TIMEOUT)
			event_queue_remove(base, ev, EVLIST_TIMEOUT);

		/* Check if it is active due to a timeout.  Rescheduling
		 * this timeout before the callback can be executed
		 * removes it from the active list. */
		if ((ev->ev_flags & EVLIST_ACTIVE) &&
		    (ev->ev_res & EV_TIMEOUT)) {
			/* See if we are just active executing this
			 * event in a loop
			 */
			if (ev->ev_ncalls && ev->ev_pncalls) {
				/* Abort loop */
				*ev->ev_pncalls = 0;
			}

			event_queue_remove(base, ev, EVLIST_ACTIVE);
		}

		gettime(base, &now);
		evutil_timeradd(&now, tv, &ev->ev_timeout);

		event_debug((
			 "event_add: timeout in %ld seconds, call %p",
			 tv->tv_sec, ev->ev_callback));

		event_queue_insert(base, ev, EVLIST_TIMEOUT);
	}

	return (res);
}


int
event_del(struct event *ev)
{
	struct event_base *base;
	const struct eventop *evsel;
	void *evbase;

	event_debug(("event_del: %p, callback %p",
		 ev, (void*)(uintptr_t)ev->ev_callback));

	/* An event without a base has not been added */
	if (ev->ev_base == NULL)
		return (-1);

	base = ev->ev_base;
	evsel = base->evsel;
	evbase = base->evbase;

	assert(!(ev->ev_flags & ~EVLIST_ALL));

	/* See if we are just active executing this event in a loop */
	if (ev->ev_ncalls && ev->ev_pncalls) {
		/* Abort loop */
		*ev->ev_pncalls = 0;
	}

	if (ev->ev_flags & EVLIST_TIMEOUT)
		event_queue_remove(base, ev, EVLIST_TIMEOUT);

	if (ev->ev_flags & EVLIST_ACTIVE)
		event_queue_remove(base, ev, EVLIST_ACTIVE);

	if (ev->ev_flags & EVLIST_INSERTED) {
		event_queue_remove(base, ev, EVLIST_INSERTED);
		return (evsel->del(evbase, ev));
	}

	return (0);
}


int event_base_set(struct event_base *base, struct event *ev){
	//只有"innocent events"可以被分配到不同的事件基（base）上。
	if (ev->ev_flags != EVLIST_INIT)
		return (-1);

	ev->ev_base = base;
	//初始化默认的优先级
	ev->ev_pri = base->nactivequeues/2;

	return (0);
}


/*
* 设置事件的优先级 - 如果事件已在活动列表中，
* 更改优先级将会失败。
*/

 int
 event_priority_set(struct event *ev, int pri)
 {
	 if (ev->ev_flags & EVLIST_ACTIVE)
		 return (-1);
	 if (pri < 0 || pri >= ev->ev_base->nactivequeues)
		 return (-1);

	 ev->ev_pri = pri;

	 return (0);
 }


/*
* 持续等待事件。只有当没有剩余事件时我们才会退出。
*/

int
event_dispatch(void)
{
	return (event_loop(0));
}


int
event_base_dispatch(struct event_base *event_base)
{
  return (event_base_loop(event_base, 0));
}


/* not thread safe */

int
event_loop(int flags)
{
	return event_base_loop(current_base, flags);
}



int
event_base_loop(struct event_base *base, int flags)
{
	const struct eventop *evsel = base->evsel;
	void *evbase = base->evbase;
	struct timeval tv;
	struct timeval *tv_p;
	int res, done;

	/* clear time cache */
	base->tv_cache.tv_sec = 0;

	if (base->sig.ev_signal_added)
		evsignal_base = base;
	done = 0;
	while (!done) {
		/* Terminate the loop if we have been asked to */
		if (base->event_gotterm) {
			base->event_gotterm = 0;
			break;
		}

		if (base->event_break) {
			base->event_break = 0;
			break;
		}

		timeout_correct(base, &tv);

		tv_p = &tv;
		if (!base->event_count_active && !(flags & EVLOOP_NONBLOCK)) {
			timeout_next(base, &tv_p);
		} else {
			/*
			 * if we have active events, we just poll new events
			 * without waiting.
			 */
			evutil_timerclear(&tv);
		}

		/* If we have no events, we just exit */
		if (!event_haveevents(base)) {
			event_debug(("%s: no events registered.", __func__));
			return (1);
		}

		/* update last old time */
		gettime(base, &base->event_tv);

		/* clear time cache */
		base->tv_cache.tv_sec = 0;

		res = evsel->dispatch(base, evbase, tv_p);

		if (res == -1)
			return (-1);
		gettime(base, &base->tv_cache);

		timeout_process(base);

		if (base->event_count_active) {
			event_process_active(base);
			if (!base->event_count_active && (flags & EVLOOP_ONCE))
				done = 1;
		} else if (flags & EVLOOP_NONBLOCK)
			done = 1;
	}

	/* clear time cache */
	base->tv_cache.tv_sec = 0;

	event_debug(("%s: asked to terminate loop.", __func__));
	return (0);
}


/* Sets up an event for processing once */

struct event_once {
	struct event ev;

	void (*cb)(int, short, void *);
	void *arg;
};


/* One-time callback, it deletes itself */

static void
event_once_cb(int fd, short events, void *arg)
{
	struct event_once *eonce = arg;

	(*eonce->cb)(fd, events, eonce->arg);
	free(eonce);
}




/* not threadsafe, event scheduled once. */
int
event_once(int fd, short events,
    void (*callback)(int, short, void *), void *arg, const struct timeval *tv)
{
	return event_base_once(current_base, fd, events, callback, arg, tv);
}

/* Schedules an event once */
int
event_base_once(struct event_base *base, int fd, short events,
    void (*callback)(int, short, void *), void *arg, const struct timeval *tv)
{
	struct event_once *eonce;
	struct timeval etv;
	int res;

	/* We cannot support signals that just fire once */
	if (events & EV_SIGNAL)
		return (-1);

	if ((eonce = calloc(1, sizeof(struct event_once))) == NULL)
		return (-1);

	eonce->cb = callback;
	eonce->arg = arg;

	if (events == EV_TIMEOUT) {
		if (tv == NULL) {
			evutil_timerclear(&etv);
			tv = &etv;
		}

		evtimer_set(&eonce->ev, event_once_cb, eonce);
	} else if (events & (EV_READ|EV_WRITE)) {
		events &= EV_READ|EV_WRITE;

		event_set(&eonce->ev, fd, events, event_once_cb, eonce);
	} else {
		/* Bad event combination */
		free(eonce);
		return (-1);
	}

	res = event_base_set(base, &eonce->ev);
	if (res == 0)
		res = event_add(&eonce->ev, tv);
	if (res != 0) {
		free(eonce);
		return (res);
	}

	return (0);
}

void
event_base_free(struct event_base *base)
{
	int i, n_deleted=0;
	struct event *ev;

	if (base == NULL && current_base)
		base = current_base;
	if (base == current_base)
		current_base = NULL;

	/* XXX(niels) - check for internal events first */
	assert(base);
	/* Delete all non-internal events. */
	for (ev = TAILQ_FIRST(&base->eventqueue); ev; ) {
		struct event *next = TAILQ_NEXT(ev, ev_next);
		if (!(ev->ev_flags & EVLIST_INTERNAL)) {
			event_del(ev);
			++n_deleted;
		}
		ev = next;
	}
	while ((ev = min_heap_top(&base->timeheap)) != NULL) {
		event_del(ev);
		++n_deleted;
	}

	for (i = 0; i < base->nactivequeues; ++i) {
		for (ev = TAILQ_FIRST(base->activequeues[i]); ev; ) {
			struct event *next = TAILQ_NEXT(ev, ev_active_next);
			if (!(ev->ev_flags & EVLIST_INTERNAL)) {
				event_del(ev);
				++n_deleted;
			}
			ev = next;
		}
	}

	if (n_deleted)
		event_debug(("%s: %d events were still set in base",
			__func__, n_deleted));

	if (base->evsel->dealloc != NULL)
		base->evsel->dealloc(base, base->evbase);

	for (i = 0; i < base->nactivequeues; ++i)
		assert(TAILQ_EMPTY(base->activequeues[i]));

	assert(min_heap_empty(&base->timeheap));
	min_heap_dtor(&base->timeheap);

	for (i = 0; i < base->nactivequeues; ++i)
		free(base->activequeues[i]);
	free(base->activequeues);

	assert(TAILQ_EMPTY(&base->eventqueue));

	free(base);
}


int
event_pending(struct event *ev, short event, struct timeval *tv)
{
	struct timeval	now, res;
	int flags = 0;

	if (ev->ev_flags & EVLIST_INSERTED)
		flags |= (ev->ev_events & (EV_READ|EV_WRITE|EV_SIGNAL));
	if (ev->ev_flags & EVLIST_ACTIVE)
		flags |= ev->ev_res;
	if (ev->ev_flags & EVLIST_TIMEOUT)
		flags |= EV_TIMEOUT;

	event &= (EV_TIMEOUT|EV_READ|EV_WRITE|EV_SIGNAL);

	/* See if there is a timeout that we should report */
	if (tv != NULL && (flags & event & EV_TIMEOUT)) {
		gettime(ev->ev_base, &now);
		evutil_timersub(&ev->ev_timeout, &now, &res);
		/* correctly remap to real time */
		evutil_gettimeofday(&now, NULL);
		evutil_timeradd(&now, &res, tv);
	}

	return (flags & event);
}



static int
timeout_next(struct event_base *base, struct timeval **tv_p)
{
	struct timeval now;
	struct event *ev;
	struct timeval *tv = *tv_p;

	if ((ev = min_heap_top(&base->timeheap)) == NULL) {
		/* if no time-based events are active wait for I/O */
		*tv_p = NULL;
		return (0);
	}

	if (gettime(base, &now) == -1)
		return (-1);

	if (evutil_timercmp(&ev->ev_timeout, &now, <=)) {
		evutil_timerclear(tv);
		return (0);
	}

	evutil_timersub(&ev->ev_timeout, &now, tv);

	assert(tv->tv_sec >= 0);
	assert(tv->tv_usec >= 0);

	event_debug(("timeout_next: in %ld seconds", tv->tv_sec));
	return (0);
}



/*
 * Determines if the time is running backwards by comparing the current
 * time against the last time we checked.  Not needed when using clock
 * monotonic.
 */

 static void
 timeout_correct(struct event_base *base, struct timeval *tv)
 {
	 struct event **pev;
	 unsigned int size;
	 struct timeval off;
 
	 if (use_monotonic)
		 return;
 
	 /* Check if time is running backwards */
	 gettime(base, tv);
	 if (evutil_timercmp(tv, &base->event_tv, >=)) {
		 base->event_tv = *tv;
		 return;
	 }

	 event_debug(("%s: time is running backwards, corrected",
			 __func__));
	 evutil_timersub(&base->event_tv, tv, &off);

	 /*
	  * We can modify the key element of the node without destroying
	  * the key, beause we apply it to all in the right order.
	  */
	 pev = base->timeheap.p;
	 size = base->timeheap.n;
	 for (; size-- > 0; ++pev) {
		 struct timeval *ev_tv = &(**pev).ev_timeout;
		 evutil_timersub(ev_tv, &off, ev_tv);
	 }
	 /* Now remember what the new time turned out to be. */
	 base->event_tv = *tv;
 }


void
timeout_process(struct event_base *base)
{
	struct timeval now;
	struct event *ev;

	if (min_heap_empty(&base->timeheap))
		return;

	gettime(base, &now);

	while ((ev = min_heap_top(&base->timeheap))) {
		if (evutil_timercmp(&ev->ev_timeout, &now, >))
			break;

		/* delete this event from the I/O queues */
		event_del(ev);

		event_debug(("timeout_process: call %p",
			 (void*)(uintptr_t)ev->ev_callback));
		event_active(ev, EV_TIMEOUT, 1);
	}
}



/* Functions for debugging */

const char *
event_get_version(void)
{
	return ("luev-1.4.1");
}

/*
 * No thread-safe interface needed - the information should be the same
 * for all threads.
 */

const char *
event_get_method(void)
{
	return (current_base->evsel->name);
}
