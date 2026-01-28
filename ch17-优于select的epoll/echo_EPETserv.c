#include <stdio.h>      // 标准I/O：printf, puts, fputs, stderr 等
#include <stdlib.h>     // exit, malloc, atoi
#include <string.h>     // memset
#include <unistd.h>     // close, read, write（POSIX I/O）
#include <fcntl.h>      // fcntl, O_NONBLOCK（设置非阻塞）
#include <errno.h>      // errno, EAGAIN（非阻塞读写的错误码判断）
#include <arpa/inet.h>  // htonl, htons（网络字节序转换）
#include <sys/socket.h> // socket, bind, listen, accept（套接字系统调用）
#include <sys/epoll.h>  // epoll_create, epoll_ctl, epoll_wait, epoll_event

#define BUF_SIZE 4      // 读写缓冲区大小（故意很小，用于演示“分多次read/write回显”）
#define EPOLL_SIZE 50   // epoll_wait 一次最多返回的事件数量（也作为 epoll_create 的 size 提示）
void setnonblockingmode(int fd);
void error_handling(char *buf);

int main(int argc, char* argv[])
{
    int serv_sock, clnt_sock;
    struct sockaddr_in serv_addr, clnt_addr;
    socklen_t clnt_addr_sz;
    int str_len;
    char buf[BUF_SIZE];

    int epfd, event_cnt;
    struct epoll_event *ep_events;
    struct epoll_event event;

    /*
     * 参数检查：要求只提供一个参数（端口）
     * argv[0]：程序名
     * argv[1]：端口号字符串
     */
    if(argc != 2)
    {
        printf("Usage: %s <port>\n", argv[0]);
        exit(1);
    }

    /*
     * 1) 创建 TCP 监听 socket
     * PF_INET：IPv4
     * SOCK_STREAM：TCP（面向连接的字节流）
     * 0：让系统自动选择协议（通常为 TCP）
     */
    serv_sock = socket(PF_INET, SOCK_STREAM, 0);
    if(serv_sock == -1)
        error_handling("socket() error");

    /*
     * 2) 准备服务器地址并 bind
     */
    memset(&serv_addr, 0, sizeof(serv_addr));     // 清零结构体，避免脏数据
    serv_addr.sin_family = AF_INET;              // IPv4
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY); // 绑定 0.0.0.0：本机所有网卡/所有IP
    serv_addr.sin_port = htons(atoi(argv[1]));   // 端口：主机序->网络序

    if(bind(serv_sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == -1)
        error_handling("bind() error");

    /*
     * 3) 进入监听状态
     * backlog=5：连接请求队列上限（等待 accept 的排队长度，不等于最大在线人数）
     */
    if(listen(serv_sock, 5) == -1)
        error_handling("listen() error");

    /*
     * 4) 创建 epoll 实例
     * epoll_create(EPOLL_SIZE)：
     * - 返回 epoll 文件描述符 epfd
     * - 参数 size 在新内核中基本是“历史遗留的提示值”，但必须 > 0
     */
    epfd = epoll_create(EPOLL_SIZE);

    /*
     * 5) 将“监听 socket”加入 epoll 关注列表
     * event.data.fd：告诉 epoll 这是哪个 fd 的事件
     * event.events = EPOLLIN：关心“可读事件”
     *
     * 对监听 socket 来说，“可读”通常表示：
     * - 有新的连接到来（accept 不会阻塞）
     */
    event.data.fd = serv_sock;
    event.events = EPOLLIN;

    /*
     * 教学重点：在 ET（边缘触发）/非阻塞模型里，fd 通常必须设置为非阻塞
     * - 监听 socket 也建议设为非阻塞（避免某些边界情况下阻塞）
     * - 本代码对 serv_sock 和 clnt_sock 都设了非阻塞
     */
    setnonblockingmode(serv_sock);

    /*
     * epoll_ctl：控制 epoll 关注的 fd 集合
     * EPOLL_CTL_ADD：添加一个 fd
     */
    epoll_ctl(epfd, EPOLL_CTL_ADD, serv_sock, &event);

    /*
     * 为 epoll_wait 分配返回事件数组：
     * - epoll_wait 会把“就绪的事件”填到这个数组里
     */
    ep_events = malloc(sizeof(struct epoll_event) * EPOLL_SIZE);

    while(1)
    {
        /*
         * 6) epoll_wait：等待 I/O 事件发生（阻塞）
         * 参数：
         * - epfd：epoll 实例
         * - ep_events：输出数组（保存就绪事件）
         * - EPOLL_SIZE：数组容量（最多返回多少个就绪事件）
         * - -1：一直阻塞直到至少有一个事件发生
         *
         * 返回值：
         * - >0：本次返回的就绪事件数量
         * - 0：超时（这里不会发生，因为 timeout=-1）
         * - -1：出错
         */
        event_cnt = epoll_wait(epfd, ep_events, EPOLL_SIZE, -1);
        if(event_cnt == -1)
        {
            puts("epoll_wait() error");
            break;
        }
        puts("return epoll_wait()");

        /*
         * 7) 遍历本次就绪的所有事件
         */
        for(int i = 0; i < event_cnt; i++)
        {
            /*
             * 如果就绪 fd 是 serv_sock：说明有新连接到来
             */
            if(ep_events[i].data.fd == serv_sock)
            {
                clnt_addr_sz = sizeof(clnt_addr);

                /*
                 * accept：接收客户端连接
                 * - 成功返回一个新的“已连接 socket”clnt_sock
                 * - clnt_addr 被填充为客户端地址
                 *
                 * 教学提示（不改代码，仅说明）：
                 * - 本代码在 accept 之后立刻 setnonblockingmode(clnt_sock)，
                 *   但 accept 失败时 clnt_sock=-1，再去 fcntl 会有风险。
                 *   更严谨的顺序应先判断 clnt_sock 是否为 -1 再设置非阻塞。
                 */
                clnt_sock = accept(serv_sock,
                    (struct sockaddr*)&clnt_addr, &clnt_addr_sz);

                /*
                 * 将客户端 socket 设为非阻塞：
                 * - ET 模式下必须读到 EAGAIN，否则可能“丢事件”导致连接卡住
                 */
                setnonblockingmode(clnt_sock);

                if(clnt_sock == -1)
                    error_handling("aceept() error"); // 这里字符串里 accept 拼写有误，但不改代码

                /*
                 * 将客户端 socket 加入 epoll 关注列表：
                 * EPOLLIN：关心读事件
                 * EPOLLET：边缘触发（Edge Triggered）
                 *
                 * 教学重点：ET vs LT
                 * - LT（默认）：只要缓冲区里还有数据没读完，epoll_wait 可能持续返回该 fd 可读
                 * - ET：只在“状态从无到有”变化时通知一次；如果你没把数据一次性读空，
                 *       可能不会再收到通知，导致数据一直留在内核缓冲里（程序看起来像“卡住”）
                 * 因此 ET 必须配合：
                 * 1) 非阻塞 fd
                 * 2) 循环 read，直到 read 返回 -1 且 errno==EAGAIN（表示已读空）
                 */
                event.events = EPOLLIN | EPOLLET;
                event.data.fd = clnt_sock;
                epoll_ctl(epfd, EPOLL_CTL_ADD, clnt_sock, &event);
                printf("Connected client: %d\n", clnt_sock);
            }
            else
            {
                /*
                 * 否则：就绪的是某个客户端 socket（可读）
                 * 在 ET 模式下，必须循环读到 EAGAIN 才能保证“读空”内核缓冲区
                 */
                while (1)
                {
                    /*
                     * read：从客户端 fd 读取数据
                     * BUF_SIZE 很小（4），所以一条较长消息会被分多次读出
                     * str_len 返回值含义：
                     * - >0：成功读到 str_len 字节
                     * - 0 ：对端关闭连接（读到 EOF），应关闭本端 socket 并从 epoll 删除
                     * - <0：出错；对非阻塞 fd 来说，如果 errno==EAGAIN 表示“暂时无数据可读”
                     */
                    str_len = read(ep_events[i].data.fd, buf, BUF_SIZE);

                    if (str_len == 0)
                    {
                        /*
                         * 客户端关闭连接（FIN）：
                         * - 从 epoll 关注列表删除该 fd
                         * - close 释放 fd
                         */
                        epoll_ctl(epfd, EPOLL_CTL_DEL,
                                  ep_events[i].data.fd, NULL);
                        close(ep_events[i].data.fd);
                        printf("Close client: %d\n", ep_events[i].data.fd);

                        /*
                         * 教学提示（不改代码，仅说明）：
                         * - 这里 close 后没有 break，会继续 while(1) 循环再 read 一次；
                         *   对已关闭 fd 再 read 可能返回 -1 且 errno=EBADF。
                         * - 更严谨通常会在关闭后 break; 退出该 fd 的读循环。
                         */
                    }
                    else if(str_len < 0)
                    {
                        /*
                         * read < 0：对非阻塞 IO，最常见是 errno==EAGAIN
                         * EAGAIN 表示：
                         * - 当前没有更多数据可读了（内核接收缓冲区已被读空）
                         * - 在 ET 模式下，这正是我们想要的“停止读取”条件
                         */
                        if(errno == EAGAIN)
                            break;

                        /*
                         * 教学提示（不改代码，仅说明）：
                         * - 如果 errno 不是 EAGAIN，说明发生了真正的错误（如 ECONNRESET）
                         *   工程上应关闭连接并从 epoll 删除。
                         * - 本代码没有处理其它错误分支，可能导致死循环或资源泄露风险。
                         */
                    }
                    else
                    {
                        /*
                         * 成功读到数据：做“回显”（echo）
                         * - 这里直接 write 回同一个客户端 fd
                         * - write 的第三个参数用 str_len，确保写回的字节数与读到一致
                         *
                         * 教学提示：
                         * - write 在非阻塞下也可能出现“部分写”（返回值 < str_len），
                         *   严谨实现需要循环写完或用缓冲队列配合 EPOLLOUT。
                         * - 这里作为示例，忽略部分写问题。
                         */
                        write(ep_events[i].data.fd, buf, str_len);
                    }
                }
            }
        }
    }

    /*
     * 退出循环后，关闭监听 socket 与 epoll fd
     * 教学提示：
     * - ep_events 由 malloc 分配，严格来说应 free(ep_events) 再退出
     * - 本示例未 free，但进程退出时 OS 会回收内存（仍建议养成良好习惯）
     */
    close(serv_sock);
    close(epfd);

    return 0;
}

/*
 * setnonblockingmode：把 fd 设置为非阻塞模式
 *
 * 非阻塞模式的意义（教学重点）：
 * - 对 read：
 *   * 没数据时不阻塞等待，而是立刻返回 -1，并设置 errno=EAGAIN/EWOULDBLOCK
 * - 对 accept：
 *   * 没有新连接时不阻塞等待，而是返回 -1 并设置 errno
 *
 * 为什么 ET 模式需要非阻塞？
 * - ET 只通知一次，如果你在一次回调/处理里没有把数据读空就阻塞住，
 *   可能导致后续再也得不到通知，从而“卡死”
 */
void setnonblockingmode(int fd)
{
	int flag=fcntl(fd, F_GETFL, 0);        // 取出当前 fd 的文件状态标志
	fcntl(fd, F_SETFL, flag|O_NONBLOCK);   // 在原有标志基础上加上 O_NONBLOCK
}

/*
 * error_handling：简单的错误处理
 * - 输出错误信息到 stderr
 * - 退出程序
 */
void error_handling(char *buf)
{
	fputs(buf, stderr);
	fputc('\n', stderr);
	exit(1);
}
