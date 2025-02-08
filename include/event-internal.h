#ifndef _EVENT_INTERNAL_H_
#define _EVENT_INTERNAL_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>

#include <event.h>


struct event_op;
struct evsig_info;
struct min_heap;


struct event_base{
    //事件处理后端select epoll poll
    const struct event_op * evsel;
    void*evbase;
    int event_count;
    int event_count_active;

    /* Set to terminate loop */
    int event_gotterm;
    /* Set to terminate loop immediately*/
    int event_break;
    int nactivequeues;

    /*Signal handing info  */
    struct evsig_info sig;
    struct event_list eventqueue;
	struct timeval event_tv;

	struct min_heap timeheap;

	struct timeval tv_cache;

};

#ifdef __cplusplus
}
#endif

#endif /* _EVENT_INTERNAL_H_ */