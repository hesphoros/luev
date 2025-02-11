
#include "env_config.h"
#include <sys/types.h>
#include <sys/time.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <assert.h>

#include "ev_event.h"
#include "ev_event-internal.h"
#include "ev_signal.h"
#include "ev_util.h"
#include "ev_log.h"

struct event_base *evsignal_base = NULL;

static void evsignal_handler(int sig);

/* Callback for when the signal handler write a byte to our signaling socket */
static void
evsignal_cb(int fd, short what, void *arg)
{
	UNUSED_PARAM(what);
	UNUSED_PARAM(arg);
	static char signals[1];
#ifdef WIN32
	SSIZE_T n;
#else
	ssize_t n;
#endif

	n = recv(fd, signals, sizeof(signals), 0);
	if (n == -1)
		event_err(1, "%s: read", __func__);
}



int
evsignal_init(struct event_base *base)
{
	int i;

	/*
	 * Our signal handler is going to write to one end of the socket
	 * pair to wake up our event loop.  The event loop then scans for
	 * signals that got delivered.
	 */
	// 创建一个UNIX域的socket对，用于信号处理程序与事件循环之间的通信
	if (evutil_socketpair(
		    AF_UNIX, SOCK_STREAM, 0, base->sig.ev_signal_pair) == -1) {
#ifdef WIN32
		/* Make this nonfatal on win32, where sometimes people
		   have localhost firewalled. */
		// 在Windows平台上，如果创建socket对失败，发出警告但不终止程序
		event_warn("%s: socketpair", __func__);
#else
		event_err(1, "%s: socketpair", __func__);
#endif
		return -1;
	}

	FD_CLOSEONEXEC(base->sig.ev_signal_pair[0]);
	FD_CLOSEONEXEC(base->sig.ev_signal_pair[1]);
	base->sig.sh_old = NULL;
	base->sig.sh_old_max = 0;
	base->sig.evsignal_caught = 0;
	memset(&base->sig.evsigcaught, 0, sizeof(sig_atomic_t)*NSIG);
	/* initialize the queues for all events */
	for (i = 0; i < NSIG; ++i)
		TAILQ_INIT(&base->sig.evsigevents[i]);

        evutil_make_socket_nonblocking(base->sig.ev_signal_pair[0]);

	event_set(&base->sig.ev_signal, base->sig.ev_signal_pair[1],
		EV_READ | EV_PERSIST, evsignal_cb, &base->sig.ev_signal);
	base->sig.ev_signal.ev_base = base;
	base->sig.ev_signal.ev_flags |= EVLIST_INTERNAL;

	return 0;
}

/* Helper: set the signal handler for evsignal to handler in base, so that
 * we can restore the original handler when we clear the current one. */
int
_evsignal_set_handler(struct event_base *base,
		      int evsignal, void (*handler)(int))
{
#ifdef HAVE_SIGACTION
	struct sigaction sa;
#else
	ev_sighandler_t sh;
#endif
	struct evsignal_info *sig = &base->sig;
	void *p;

	/*
	 * resize saved signal handler array up to the highest signal number.
	 * a dynamic array is used to keep footprint on the low side.
	 */
	if (evsignal >= sig->sh_old_max) {
		int new_max = evsignal + 1;
		event_debug(("%s: evsignal (%d) >= sh_old_max (%d), resizing",
			    __func__, evsignal, sig->sh_old_max));
		p = realloc(sig->sh_old, new_max * sizeof(*sig->sh_old));
		if (p == NULL) {
			event_warn("realloc");
			return (-1);
		}

		memset((char *)p + sig->sh_old_max * sizeof(*sig->sh_old),
		    0, (new_max - sig->sh_old_max) * sizeof(*sig->sh_old));

		sig->sh_old_max = new_max;
		sig->sh_old = p;
	}

	/* allocate space for previous handler out of dynamic array */
	sig->sh_old[evsignal] = malloc(sizeof *sig->sh_old[evsignal]);
	if (sig->sh_old[evsignal] == NULL) {
		event_warn("malloc");
		return (-1);
	}

	/* save previous handler and setup new handler */
#ifdef HAVE_SIGACTION
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = handler;
	sa.sa_flags |= SA_RESTART;
	sigfillset(&sa.sa_mask);

	if (sigaction(evsignal, &sa, sig->sh_old[evsignal]) == -1) {
		event_warn("sigaction");
		free(sig->sh_old[evsignal]);
		return (-1);
	}
#else
	if ((sh = signal(evsignal, handler)) == SIG_ERR) {
		event_warn("signal");
		free(sig->sh_old[evsignal]);
		return (-1);
	}
	*sig->sh_old[evsignal] = sh;
#endif

	return (0);
}

int
evsignal_add(struct event *ev)
{
	int evsignal;
	struct event_base *base = ev->ev_base;
	struct evsignal_info *sig = &ev->ev_base->sig;

	if (ev->ev_events & (EV_READ|EV_WRITE))
		event_errx(1, "%s: EV_SIGNAL incompatible use", __func__);
	evsignal = EVENT_SIGNAL(ev);
	assert(evsignal >= 0 && evsignal < NSIG);
	if (TAILQ_EMPTY(&sig->evsigevents[evsignal])) {
		event_debug(("%s: %p: changing signal handler", __func__, ev));
		if (_evsignal_set_handler(
			    base, evsignal, evsignal_handler) == -1)
			return (-1);

		/* catch signals if they happen quickly */
		evsignal_base = base;

		if (!sig->ev_signal_added) {
			if (event_add(&sig->ev_signal, NULL))
				return (-1);
			sig->ev_signal_added = 1;
		}
	}

	/* multiple events may listen to the same signal */
	TAILQ_INSERT_TAIL(&sig->evsigevents[evsignal], ev, ev_signal_next);

	return (0);
}

int
_evsignal_restore_handler(struct event_base *base, int evsignal)
{
	int ret = 0;
	struct evsignal_info *sig = &base->sig;
#ifdef HAVE_SIGACTION
	struct sigaction *sh;
#else
	ev_sighandler_t *sh;
#endif

	/* restore previous handler */
	sh = sig->sh_old[evsignal];
	sig->sh_old[evsignal] = NULL;
#ifdef HAVE_SIGACTION
	if (sigaction(evsignal, sh, NULL) == -1) {
		event_warn("sigaction");
		ret = -1;
	}
#else
	if (signal(evsignal, *sh) == SIG_ERR) {
		event_warn("signal");
		ret = -1;
	}
#endif
	free(sh);

	return ret;
}

int
evsignal_del(struct event *ev)
{
	struct event_base *base = ev->ev_base;
	struct evsignal_info *sig = &base->sig;
	int evsignal = EVENT_SIGNAL(ev);

	assert(evsignal >= 0 && evsignal < NSIG);

	/* multiple events may listen to the same signal */
	TAILQ_REMOVE(&sig->evsigevents[evsignal], ev, ev_signal_next);

	if (!TAILQ_EMPTY(&sig->evsigevents[evsignal]))
		return (0);

	event_debug(("%s: %p: restoring signal handler", __func__,(void*) ev));

	return (_evsignal_restore_handler(ev->ev_base, EVENT_SIGNAL(ev)));
}

static void
evsignal_handler(int sig)
{
	int save_errno = errno;

	if (evsignal_base == NULL) {
		event_warn(
			"%s: received signal %d, but have no base configured",
			__func__, sig);
		return;
	}

	evsignal_base->sig.evsigcaught[sig]++;
	evsignal_base->sig.evsignal_caught = 1;

#ifndef HAVE_SIGACTION
	signal(sig, evsignal_handler);
#endif

	/* Wake up our notification mechanism */
	send(evsignal_base->sig.ev_signal_pair[0], "a", 1, 0);
	errno = save_errno;
}


void
evsignal_process(struct event_base *base)
{
	struct evsignal_info *sig = &base->sig;
	struct event *ev, *next_ev;
	sig_atomic_t ncalls;
	int i;

	base->sig.evsignal_caught = 0;
	for (i = 1; i < NSIG; ++i) {
		ncalls = sig->evsigcaught[i];
		if (ncalls == 0)
			continue;
		sig->evsigcaught[i] -= ncalls;

		for (ev = TAILQ_FIRST(&sig->evsigevents[i]);
		    ev != NULL; ev = next_ev) {
			next_ev = TAILQ_NEXT(ev, ev_signal_next);
			if (!(ev->ev_events & EV_PERSIST))
				event_del(ev);
			event_active(ev, EV_SIGNAL, ncalls);
		}

	}
}

void
evsignal_dealloc(struct event_base *base)
{
	int i = 0;
	if (base->sig.ev_signal_added) {
		event_del(&base->sig.ev_signal);
		base->sig.ev_signal_added = 0;
	}
	for (i = 0; i < NSIG; ++i) {
		if (i < base->sig.sh_old_max && base->sig.sh_old[i] != NULL)
			_evsignal_restore_handler(base, i);
	}

	EVUTIL_CLOSESOCKET(base->sig.ev_signal_pair[0]);
	base->sig.ev_signal_pair[0] = -1;
	EVUTIL_CLOSESOCKET(base->sig.ev_signal_pair[1]);
	base->sig.ev_signal_pair[1] = -1;
	base->sig.sh_old_max = 0;

	/* per index frees are handled in evsignal_del() */
	free(base->sig.sh_old);
}
