#ifndef _LU_EV_POLL_H_
#define _LU_EV_POLL_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/poll.h>
#include <sys/queue.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>

#include "ev_event.h"
#include "ev_event-internal.h"
#include "ev_signal.h"
#include "ev_log.h"

struct pollop{
    int event_count;		/* Highest number alloc */
	int nfds;                       /* Size of event_* */
	int fd_count;                   /* Size of idxplus1_by_fd */
	struct pollfd *event_set;
	struct event **event_r_back;
	struct event **event_w_back;
	int *idxplus1_by_fd; /* Index into event_set by fd; we add 1 so
			      * that 0 (which is easy to memset) can mean
			      * "no entry." */
};

static void *poll_init	(struct event_base *);
static int poll_add		(void *, struct event *);
static int poll_del		(void *, struct event *);
static int poll_dispatch	(struct event_base *, void *, struct timeval *);
static void poll_dealloc	(struct event_base *, void *);


extern const struct eventop pollops;



#ifdef __cplusplus
}
#endif
#endif /* _LU_EV_POLL_H_ */