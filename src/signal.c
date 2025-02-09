#include "common.h"
#include "event-internal.h"
#include "evutil.h"
#include "evsignal.h"
#include "log.h"
#include <signal.h>


struct event_base *evsignal_base = NULL;
static void evsignal_handler(int sig);
static void evsignal_cb(int fd, short what, void *arg);


int
_evsignal_set_handler(struct event_base *base,int evsignal, void (*handler)(int));
int
_evsignal_restore_handler(struct event_base *base, int evsignal);
#define error_is_eagain(err) ((err) == EAGAIN)


int
evsignal_init(struct event_base *base)
{
	int i;
    /**
     * 我们的信号处理程序将写入套接字对的一端以唤醒我们的事件循环。
     * 然后事件循环扫描已传递的信号。
    */
	// 创建一个UNIX域的socket对，用于信号处理
	if (evutil_socketpair(
		    AF_UNIX, SOCK_STREAM, 0, base->sig.ev_signal_pair) == -1) {
		// 在非Windows平台上，如果创建socket对失败，发出错误并终止程序
		event_err(1, "%s: socketpair", __func__);
		return -1;
	}

	// 设置socket对在执行exec时自动关闭
	FD_CLOSEONEXEC(base->sig.ev_signal_pair[0]);
	FD_CLOSEONEXEC(base->sig.ev_signal_pair[1]);
	// 初始化信号处理相关的变量
	base->sig.sh_old = NULL;
	base->sig.sh_old_max = 0;
	base->sig.evsignal_caught = 0;
	// 清零所有信号捕获标志数组
	memset(&base->sig.evsigcaught, 0, sizeof(sig_atomic_t)*NSIG);
	/* initialize the queues for all events */
	for (i = 0; i < NSIG; ++i)
		TAILQ_INIT(&base->sig.evsigevents[i]);

        evutil_make_socket_nonblocking(base->sig.ev_signal_pair[0]);
        evutil_make_socket_nonblocking(base->sig.ev_signal_pair[1]);

	event_set(&base->sig.ev_signal, base->sig.ev_signal_pair[1],
		EV_READ | EV_PERSIST, evsignal_cb, &base->sig.ev_signal);
	base->sig.ev_signal.ev_base = base;
	base->sig.ev_signal.ev_flags |= EVLIST_INTERNAL;

	return 0;
}


// 定义一个静态函数evsignal_cb，用于处理信号回调
static void
evsignal_cb(int fd, short what, void *arg)
{
	UNUSED_PARAM(what);
	UNUSED_PARAM(arg);
	// 读取信号数据
    //存储接收到的信号数据
	static char signals[1];
    // 根据操作系统定义不同的数据类型SSIZE_T或ssize_t

	ssize_t n;
    // 使用recv函数从文件描述符fd中接收数据到signals数组中
    // sizeof(signals)表示接收数据的最大字节数
    // 0表示没有特殊的接收标志
	n = recv(fd, signals, sizeof(signals), 0);
    // 检查recv函数的返回值，如果返回-1表示接收失败
	if (n == -1) {
        // 获取错误码
		int err = EVUTIL_SOCKET_ERROR();
        // 检查错误码是否为EAGAIN或EWOULDBLOCK，这两个错误码表示资源暂时不可用
		if (! error_is_eagain(err))
            // 如果不是EAGAIN或EWOULDBLOCK错误，则打印错误信息并退出
			event_err(1, "%s: read", __func__);
    }
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


	signal(sig, evsignal_handler);
	/* Wake up our notification mechanism */
	send(evsignal_base->sig.ev_signal_pair[0], "a", 1, 0);
	errno = save_errno;
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

	event_debug(("%s: %p: restoring signal handler", __func__, ev));

	return (_evsignal_restore_handler(ev->ev_base, EVENT_SIGNAL(ev)));
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

	if (base->sig.ev_signal_pair[0] != -1) {
		EVUTIL_CLOSESOCKET(base->sig.ev_signal_pair[0]);
		base->sig.ev_signal_pair[0] = -1;
	}
	if (base->sig.ev_signal_pair[1] != -1) {
		EVUTIL_CLOSESOCKET(base->sig.ev_signal_pair[1]);
		base->sig.ev_signal_pair[1] = -1;
	}
	base->sig.sh_old_max = 0;

	/* per index frees are handled in evsig_del() */
	if (base->sig.sh_old) {
		free(base->sig.sh_old);
		base->sig.sh_old = NULL;
	}
}



/* Helper: set the signal handler for evsignal to handler in base, so that
 * we can restore the original handler when we clear the current one. */
int
_evsignal_set_handler(struct event_base *base,
		      int evsignal, void (*handler)(int))
{

	struct sigaction sa;



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

	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = handler;
	sa.sa_flags |= SA_RESTART;
	sigfillset(&sa.sa_mask);

	if (sigaction(evsignal, &sa, sig->sh_old[evsignal]) == -1) {
		event_warn("sigaction");
		free(sig->sh_old[evsignal]);
		sig->sh_old[evsignal] = NULL;
		return (-1);
	}


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
