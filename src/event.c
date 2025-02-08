#include "event.h"
#include "event-internal.h"
#include <unistd.h>
#include "signal.h"
#include "epoll.h"

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




struct event_base *event_init(void){

}



struct event_base *event_base_new(void){
    int i;
	struct event_base *base;

	if ((base = calloc(1, sizeof(struct event_base))) == NULL)
		event_err(1, "%s: calloc", __func__);

	event_sigcb = NULL;
	event_gotsig = 0;

	detect_monotonic();
	gettime(base, &base->event_tv);

	min_heap_ctor(&base->timeheap);
	TAILQ_INIT(&base->eventqueue);
	base->sig.ev_signal_pair[0] = -1;
	base->sig.ev_signal_pair[1] = -1;

	base->evbase = NULL;
	for (i = 0; eventops[i] && !base->evbase; i++) {
		base->evsel = eventops[i];

		base->evbase = base->evsel->init(base);
	}

	if (base->evbase == NULL)
		event_errx(1, "%s: no event mechanism available", __func__);

	if (evutil_getenv("EVENT_SHOW_METHOD"))
		event_msgx("libevent using: %s\n",
			   base->evsel->name);

	/* allocate a single active event queue */
	event_base_priority_init(base, 1);

	return (base);
}
