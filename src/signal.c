#include "signal.h"
#include "common.h"
#include "event-internal.h"
#include "evutil.h"
#include "evsignal.h"
#include "log.h"

static void evsignal_cb(int fd, short what, void *arg);

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
