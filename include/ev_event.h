#ifndef _EVENT_H_
#define _EVENT_H_

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>
#include <sys/queue.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>

#define EVLIST_TIMEOUT 0x01  // event在time堆中
#define EVLIST_INSERTED 0x02 // event在已注册事件链表中
#define EVLIST_SIGNAL 0x04   // event在信号事件链表中
#define EVLIST_ACTIVE 0x08   // event在激活链表中
#define EVLIST_INTERNAL 0x10 // 内部使用标记
#define EVLIST_INIT 0x80     // event初始化标记

/* EVLIST_X_ Private space: 0x1000-0xf000 */
#define EVLIST_ALL (0xf000 | 0x9f)

#define EV_TIMEOUT 0x01 // 超时
#define EV_READ 0x02    // 读
#define EV_WRITE 0x04   // 写
#define EV_SIGNAL 0x08  // 信号
#define EV_PERSIST 0x10 /* Persistant event */

    struct event_base;
    TAILQ_HEAD(event_list, event);
    struct event
    {
        // 已注册事件链表
        TAILQ_ENTRY(event)
        ev_next;
        // 激活事件链表
        TAILQ_ENTRY(event)
        ev_active_next;
        // 信号事件链表
        TAILQ_ENTRY(event)
        ev_signal_next;

        // 最小时间堆索引
        unsigned int min_heap_idx;
        // 事件基
        struct event_base *ev_base;
        // 文件描述符 or 信号
        int ev_fd;

        // 事件类型 超时 读写 信号 永久
        short ev_events;
        // 事件就绪执行时，调用 ev_callback 的次数 通常为1
        short ev_ncalls;
        // 指向 ev_ncalls 的指针，用于在回调函数中删除事件
        short *ev_pncalls;

        struct timeval ev_timeout;

        int ev_pri; /* smaller numbers are higher priority */

        /// @brief 回调函数
        /// @param fd 文件描述符 or 信号
        /// @param arg 回调函数参数
        void (*ev_callback)(int, short, void *arg);
        void *ev_arg;

        // 回调函数中传递的结果
        int ev_res; /* result passed to event callback */

        // 标记 event 信息的字段
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

    /*
     * Key-Value pairs.  Can be used for HTTP headers but also for
     * query argument parsing.
     */
    struct evkeyval
    {
        TAILQ_ENTRY(evkeyval)
        next;

        char *key;
        char *value;
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
      Reinitialized the event base after a fork

     某些事件机制无法跨 fork 存活。事件库需要使用 event_reinit() 函数重新初始化。

      @param base the event base that needs to be re-initialized
      @return 0 if successful, or -1 if some events could not be re-added.
      @see event_base_new(), event_init()
    */
    int event_reinit(struct event_base *base);

    /**
      Loop to process events.

        为了处理事件，应用程序需要调用
        event_dispatch()。此函数仅在发生错误时返回，并且应该
        替换应用程序的事件核心。

      @see event_base_dispatch()
     */
    int event_dispatch(void);

    /**
        解除与 event_base 关联的所有内存分配，并释放该基址。

        请注意，此函数不会关闭任何 fds 或释放作为回调参数传递给 event_set 的任何内存。
      @param eb an event_base to be freed
     */

    void event_base_free(struct event_base *);

    /**
      Threadsafe event dispatching loop.

      @param eb the event_base structure returned by event_init()
      @see event_init(), event_dispatch()
     */
    int event_base_dispatch(struct event_base *);

    int event_base_priority_init(struct event_base *base, int npriorities);

/**
     * 准备要添加的事件结构。
    函数 event_set() 准备事件结构 ev，以用于将来对 event_add() 和 event_del() 的调用。
    事件将准备调用 fn 参数指定的函数，该函数带有一个表示文件描述符的 int 参数、
    一个表示事件类型的 short 参数和一个在 arg 参数中给出的 void * 参数。
    fd 表示应监视事件的文件描述符。事件可以是 EV_READ、EV_WRITE 或两者。
    表示应用程序可以分别从文件描述符读取或写入而不会阻塞。

    将使用触发事件的文件描述符和事件类型（EV_TIMEOUT、EV_SIGNAL、EV_READ 或 EV_WRITE）调用函数 fn。
    附加标志 EV_PERSIST 使 event_add() 持久化，直到调用 event_del()。

将事件添加到受监控事件集。

    函数 event_add() 会在 event_set() 中指定的事件发生时或至少在 tv 中指定的时间内安排执行 ev 事件。
    如果 tv 为 NULL，则不会发生超时，并且只有当文件描述符上发生匹配事件时才会调用该函数。
    ev 参数中的事件必须已由 event_set() 初始化，并且不能在对 event_set() 的调用中使用，直到它超时或被 event_del() 删除。
    如果 ev 参数中的事件已经安排了超时，则旧的超时将被新的超时替换。
*/
/// @brief 设置事件ev绑定的fd or signal,对于定时事件设为-1即可
/// @param ev 需要设置的event
/// @param fd file discriptor or signal
/// @param events 事件类型（读、写、等）EV_READ|EV_PERSIST, EV_WRITE, EV_SIGNAL
/// @param callback 回调函数
/// @param arg 函调函数参数
void event_set(struct event *ev, int fd, short events, void (*callback)(int, short, void *), void *arg);

/**
    设置 event ev 将要注册到的 event_base
    如果一个进程中存在多个 libevent 实例，
    则必须要调用该函数为 event 设置不同的 event_base
    @param eb the event base
    @param ev the event
    */
int event_base_set(struct event_base *, struct event *);

//TODO:
//event_process_active
/// @brief 将事件标记为活动状态，并插入到活动队列中
/// @param ev event_base 结构体的指针
/// @param res result
/// @param ncalls 调用callback的次数
void event_active(struct event *ev, int res, short ncalls);

#define _EVENT_LOG_DEBUG 0
#define _EVENT_LOG_MSG 1
#define _EVENT_LOG_WARN 2
#define _EVENT_LOG_ERR 3
    typedef void (*event_log_cb)(int severity, const char *msg);

#define EVENT_SIGNAL(ev) (int)(ev)->ev_fd
#define EVENT_FD(ev) (int)(ev)->ev_fd

/**
 event_loop() flags
 */
/*@{*/
#define EVLOOP_ONCE 0x01     /**< Block at most once. */
#define EVLOOP_NONBLOCK 0x02 /**< Do not block. */
    /*@}*/

    /**
      Handle events.

      This is a more flexible version of event_dispatch().

      @param flags any combination of EVLOOP_ONCE | EVLOOP_NONBLOCK
      @return 0 if successful, -1 if an error occurred, or 1 if no events were
        registered.
      @see event_loopexit(), event_base_loop()
    */
    int event_loop(int);

    /**
      Handle events (threadsafe version).

      This is a more flexible version of event_base_dispatch().

      @param eb the event_base structure returned by event_init()
      @param flags any combination of EVLOOP_ONCE | EVLOOP_NONBLOCK
      @return 0 if successful, -1 if an error occurred, or 1 if no events were
        registered.
      @see event_loopexit(), event_base_loop()
      */
    int event_base_loop(struct event_base *, int);

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
        检查特定事件是否处于待处理或已安排状态。

      @param ev an event struct previously passed to event_add()
      @param event the requested event type; any of EV_TIMEOUT|EV_READ|
             EV_WRITE|EV_SIGNAL
      @param tv an alternate timeout (FIXME - is this true?)

      @return 1 if the event is pending, or 0 if the event has not occurred

     */
    int event_pending(struct event *ev, short event, struct timeval *tv);

    /**
        为事件分配优先级。

      @param ev an event struct
      @param priority the new priority to be assigned
      @return 0 if successful, or -1 if an error occurred
      @see event_priority_init()
      */
    int event_priority_set(struct event *, int);

    const char *event_get_method(void);

    const char *event_get_version(void);

    void timeout_process(struct event_base *base);
/**
  Add a timer event.

  @param ev the event struct
  @param tv timeval struct
 */
#define evtimer_add(ev, tv) event_add(ev, tv)

/**
  Define a timer event.

  @param ev event struct to be modified
  @param cb callback function
  @param arg argument that will be passed to the callback function
 */
#define evtimer_set(ev, cb, arg) event_set(ev, -1, 0, cb, arg)

/**
 * Delete a timer event.
 *
 * @param ev the event struct to be disabled
 */
#define evtimer_del(ev) event_del(ev)
#define evtimer_pending(ev, tv) event_pending(ev, EV_TIMEOUT, tv)
#define evtimer_initialized(ev) ((ev)->ev_flags & EVLIST_INIT)

/**
 * Add a timeout event.
 *
 * @param ev the event struct to be disabled
 * @param tv the timeout value, in seconds
 */
#define timeout_add(ev, tv) event_add(ev, tv)

/**
 * Define a timeout event.
 *
 * @param ev the event struct to be defined
 * @param cb the callback to be invoked when the timeout expires
 * @param arg the argument to be passed to the callback
 */
#define timeout_set(ev, cb, arg) event_set(ev, -1, 0, cb, arg)

/**
 * Disable a timeout event.
 *
 * @param ev the timeout event to be disabled
 */
#define timeout_del(ev) event_del(ev)

#define timeout_pending(ev, tv) event_pending(ev, EV_TIMEOUT, tv)
#define timeout_initialized(ev) ((ev)->ev_flags & EVLIST_INIT)

#define signal_add(ev, tv) event_add(ev, tv)
#define signal_set(ev, x, cb, arg) \
    event_set(ev, x, EV_SIGNAL | EV_PERSIST, cb, arg)
#define signal_del(ev) event_del(ev)
#define signal_pending(ev, tv) event_pending(ev, EV_SIGNAL, tv)
#define signal_initialized(ev) ((ev)->ev_flags & EVLIST_INIT)

/**
     Schedule a one-time event to occur.

    The function event_once() is similar to event_set().  However, it schedules
    a callback to be called exactly once and does not require the caller to
    prepare an event structure.

    @param fd a file descriptor to monitor
    @param events event(s) to monitor; can be any of EV_TIMEOUT | EV_READ |
            EV_WRITE
    @param callback callback function to be invoked when the event occurs
    @param arg an argument to be passed to the callback function
    @param timeout the maximum amount of time to wait for the event, or NULL
            to wait forever
    @return 0 if successful, or -1 if an error occurred
    @see event_set()

    */
int event_once(int, short, void (*)(int, short, void *), void *,
                const struct timeval *);
/**
  Schedule a one-time event (threadsafe variant)

  The function event_base_once() is similar to event_set().  However, it
  schedules a callback to be called exactly once and does not require the
  caller to prepare an event structure.

  @param base an event_base returned by event_init()
  @param fd a file descriptor to monitor
  @param events event(s) to monitor; can be any of EV_TIMEOUT | EV_READ |
         EV_WRITE
  @param callback callback function to be invoked when the event occurs
  @param arg an argument to be passed to the callback function
  @param timeout the maximum amount of time to wait for the event, or NULL
         to wait forever
  @return 0 if successful, or -1 if an error occurred
  @see event_once()
 */
int event_base_once(struct event_base *base, int fd, short events,
    void (*callback)(int, short, void *), void *arg,
    const struct timeval *timeout);

#ifdef __cplusplus
}
#endif

#endif /* _EVENT_H_ */