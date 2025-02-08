#include "epoll.h"


static void *
    epoll_init(struct event_base *base)
{
	int epfd;
	struct epollop *epollop;

	/* Disable epollueue when this environment variable is set */
	if (evutil_getenv("EVENT_NOEPOLL"))
		return (NULL);

	/* Initalize the kernel queue */
	if ((epfd = epoll_create(32000)) == -1) {
		if (errno != ENOSYS)
			event_warn("epoll_create");
		return (NULL);
	}

	FD_CLOSEONEXEC(epfd);

	if (!(epollop = calloc(1, sizeof(struct epollop))))
		return (NULL);

	epollop->epfd = epfd;

	/* Initalize fields */
	epollop->events = malloc(INITIAL_NEVENTS * sizeof(struct epoll_event));
	if (epollop->events == NULL) {
		free(epollop);
		return (NULL);
	}
	epollop->nevents = INITIAL_NEVENTS;

	epollop->fds = calloc(INITIAL_NFILES, sizeof(struct evepoll));
	if (epollop->fds == NULL) {
		free(epollop->events);
		free(epollop);
		return (NULL);
	}
	epollop->nfds = INITIAL_NFILES;

	evsignal_init(base);

	return (epollop);
}
