#include "ev_event.h"
#include "ev_event-internal.h"
#include "ev_epoll.h"
#include "ev_signal.h"
#include "ev_util.h"
#include "ev_log.h"

// 定义主函数，程序从这里开始执行
int main()
{
    struct event_base *base = event_base_new();
    if(base)
        printf("Hello, luevent and event_base!\n");
    // 返回0，表示程序正常结束
    return 0;
}