#ifndef _EVSIGNAL_H_
#define _EVSIGNAL_H_

#ifdef __cplusplus
extern "C" {
#endif
#include <signal.h>
#include "event.h"
#include <sys/signalfd.h>
#include <sys/signal.h>


#ifndef NSIG
#ifdef _NSIG
#define NSIG _NSIG
#else
#define NSIG 32
#endif
#endif


struct event;

struct evsignal_info {
    struct event ev_signal;                  // 监听信号通知的内部事件
    int ev_signal_pair[2];                   // 用于信号通知的套接字对（管道或 socketpair）
    int ev_signal_added;                     // 标记内部事件是否已添加到事件循环
    volatile sig_atomic_t evsignal_caught;   // 标记是否有信号被捕获（原子操作）
    struct event_list evsigevents[NSIG];     // 每个信号对应的事件队列（NSIG 是信号总数）
    sig_atomic_t evsigcaught[NSIG];          // 记录每个信号的触发次数
    struct sigaction **sh_old;               // 保存旧的信号处理函数（使用 sigaction）
    int sh_old_max;                          // sh_old 数组的容量
};

int     evsignal_init(struct event_base *);
void evsignal_process(struct event_base *);
int evsignal_add(struct event *);
int evsignal_del(struct event *);
void evsignal_dealloc(struct event_base *);


#define FD_CLOSEONEXEC(x) do { \
    if (fcntl(x, F_SETFD, FD_CLOEXEC) == -1) \
            event_warn("fcntl(%d, F_SETFD)", x); \
} while (0)



#ifdef __cplusplus
}
#endif

#endif /* _EVSIGNAL_H_ */