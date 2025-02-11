#include "ev_event.h"
#include "ev_event-internal.h"
#include "ev_epoll.h"
#include "ev_signal.h"
#include "ev_util.h"
#include "ev_log.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>

// 回调函数原型
void accept_cb(int fd, short event, void *arg);
void client_read_cb(int fd, short event, void *arg);

// 主函数
int main()
{
    struct event_base *base = event_base_new();


    // 创建一个server socket
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0)
    {
        perror("create socket error");
        return -1;
    }

    // 设置socket选项
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // 绑定地址
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(8080);
    if (bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("bind socket error");
        return -1;
    }

    // 监听连接
    if (listen(server_fd, 10) < 0)
    {
        perror("listen socket error");
        return -1;
    }

    struct event ev;
    // 设置监听socket为可读
    event_set(&ev, server_fd, EV_READ | EV_PERSIST, accept_cb, base);
    event_base_set(base, &ev);

    // 启动事件循环
    if (event_add(&ev, NULL) < 0)
    {
        perror("event_add failed");
        close(server_fd);
        event_base_free(base);
        return -1;
    }

    // 启动事件循环
    event_base_dispatch(base);

    // 清理资源
    close(server_fd);
    event_base_free(base);
    return 0;
}

// 接受客户端连接的回调
void accept_cb(int fd, short event, void *arg)
{
    UNUSED_PARAM(event);

    struct event_base *base = (struct event_base *)arg;

    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    int client_fd = accept(fd, (struct sockaddr*)&client_addr, &client_addr_len);
    if (client_fd < 0)
    {
        perror("accept error");
        return;
    }
    printf("Accepted a client from %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

    // 创建一个新的event来处理客户端的数据读取
    struct event *client_event = (struct event*)malloc(sizeof(struct event));
    if (client_event == NULL)
    {
        perror("malloc error");
        close(client_fd);
        return;
    }

    // 设置客户端的事件处理回调
    event_set(client_event, client_fd, EV_READ | EV_PERSIST, client_read_cb, (void*)client_event);
    event_base_set(base, client_event);

    // 将新的事件添加到事件循环
    if (event_add(client_event, NULL) < 0)
    {
        perror("event_add failed");
        close(client_fd);
        free(client_event);
        return;
    }
}

// 处理客户端读取数据的回调函数
void client_read_cb(int fd, short event, void *arg)
{
    UNUSED_PARAM(event);

    struct event *client_event = (struct event*)arg;
    int client_fd = fd; // 从 arg 中取出 client_fd
    char buf[1024];
    int len = read(client_fd, buf, sizeof(buf));
    if (len < 0)
    {
        perror("read error");
        close(client_fd);
        free(client_event);
        return;
    }
    if (len == 0) // 客户端关闭连接
    {
        printf("Client disconnected\n");
        close(client_fd);
        free(client_event);
        return;
    }
    printf("Received data: %s\n", buf);

    // 响应客户端（可选）
    const char *response = "Message received";
    write(client_fd, response, strlen(response));

    // 关闭客户端连接
    close(client_fd);

    // 释放事件资源
    free(client_event);
}
