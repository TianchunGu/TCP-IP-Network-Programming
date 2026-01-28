#include <stdio.h>      // 标准I/O：printf, puts, fputs, stderr 等
#include <stdlib.h>     // exit, malloc, atoi
#include <string.h>     // memset
#include <unistd.h>     // read, write, close（POSIX 系统调用）
#include <arpa/inet.h>  // htonl, htons（网络字节序转换）
#include <sys/socket.h> // socket, bind, listen, accept（套接字系统调用）
#include <sys/epoll.h>  // epoll_create, epoll_ctl, epoll_wait 以及 epoll_event

#define BUF_SIZE 100    // 读写缓冲区大小：一次最多读 100 字节再回显给客户端
#define EPOLL_SIZE 50   // epoll_wait 一次最多返回 50 个就绪事件（也用于分配事件数组）
void error_handling(char *buf);

int main(int argc, char* argv[])
{
    int serv_sock, clnt_sock;
    struct sockaddr_in serv_addr, clnt_addr;
    socklen_t clnt_addr_sz;
    int str_len;
    char buf[BUF_SIZE];

    int epfd, event_cnt;
    struct epoll_event event;
    struct epoll_event* ep_events;

    /*
     * 参数检查：
     * - 程序需要一个参数：监听端口
     * argv[0]：程序名
     * argv[1]：端口号字符串
     */
    if(argc != 2)
    {
        printf("Usage: %s <port>\n", argv[0]);
        exit(1);
    }

    /* -------------------- 第一部分：建立 TCP 监听 socket -------------------- */

    /*
     * socket(PF_INET, SOCK_STREAM, 0)
     * - PF_INET：IPv4 协议族
     * - SOCK_STREAM：面向连接的字节流服务 -> TCP
     * - 0：自动选择协议（通常为 TCP）
     *
     * 返回值：
     * - 成功：非负整数（文件描述符 FD）
     * - 失败：-1
     */
    serv_sock = socket(PF_INET, SOCK_STREAM, 0);
    if(serv_sock == -1)
        error_handling("socket() error");

    /*
     * 准备服务器地址结构体：
     * memset 清零是为了避免结构体中未初始化字段造成不可预期行为
     */
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;                 // IPv4
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);  // 绑定 0.0.0.0（本机所有 IP）
    /*
     * INADDR_ANY 表示：服务器监听本机“所有网卡/所有 IP 地址”
     * htonl：host to network long，把 32 位整数转为网络字节序（大端序）
     */
    serv_addr.sin_port = htons(atoi(argv[1]));      // 端口号（网络字节序）
    /*
     * atoi(argv[1])：端口字符串 -> 整数（主机字节序）
     * htons：host to network short，把 16 位端口转为网络字节序（大端序）
     */

    /*
     * bind：把监听 socket 绑定到本地 IP:端口
     * 典型失败原因：
     * - 端口被占用（Address already in use）
     * - 权限不足（绑定 < 1024 端口需要管理员权限）
     */
    if(bind(serv_sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == -1)
        error_handling("bind() error");

    /*
     * listen：将 serv_sock 置为监听状态
     * backlog=5：连接请求等待队列长度上限（还没被 accept 的连接排队）
     */
    if(listen(serv_sock, 5) == -1)
        error_handling("listen() error");

    /* -------------------- 第二部分：创建 epoll 并注册监听 socket -------------------- */

    /*
     * epoll_create(EPOLL_SIZE)
     * - 创建一个 epoll 实例，返回 epoll 文件描述符 epfd
     * - 参数 EPOLL_SIZE 在现代 Linux 中主要是“历史遗留的提示值”，只要求 > 0
     */
    epfd = epoll_create(EPOLL_SIZE);

    /*
     * 分配 epoll_wait 的输出事件数组：
     * - epoll_wait 会把就绪事件填入 ep_events
     * - 一次最多 EPOLL_SIZE 个事件
     */
    ep_events = malloc(sizeof(struct epoll_event) * EPOLL_SIZE);

    /*
     * 注册监听 socket 到 epoll：
     * event.events = EPOLLIN：关心“可读事件”
     *
     * 对监听 socket 而言，EPOLLIN 通常表示：
     * - 有新连接请求到来（accept 将不会阻塞或很快返回）
     */
    event.events = EPOLLIN;
    event.data.fd = serv_sock;

    /*
     * epoll_ctl：
     * EPOLL_CTL_ADD：把 serv_sock 添加到 epoll 监听集合
     */
    epoll_ctl(epfd, EPOLL_CTL_ADD, serv_sock, &event);

    /* -------------------- 第三部分：事件循环（I/O 多路复用） -------------------- */

    while(1)
    {
        /*
         * epoll_wait(epfd, ep_events, EPOLL_SIZE, -1)
         * - 等待就绪事件发生（阻塞）
         * - timeout=-1 表示无限等待，直到至少一个 fd 就绪
         *
         * 返回值 event_cnt：
         * - >0：本次返回的就绪事件数量
         * -  0：超时（这里不会发生，因为 timeout=-1）
         * - -1：出错
         */
        event_cnt = epoll_wait(epfd, ep_events, EPOLL_SIZE, -1);
        if(event_cnt == -1)
        {
            puts("epoll_wait() error");
            break;
        }

        /*
         * 逐个处理本次返回的所有就绪事件
         */
        for(int i = 0; i < event_cnt; i++)
        {
            /*
             * 如果就绪的 fd 是 serv_sock：
             * 说明有新客户端连接到达，需要 accept() 建立连接
             */
            if(ep_events[i].data.fd == serv_sock)
            {
                clnt_addr_sz = sizeof(clnt_addr);

                /*
                 * accept：接受一个客户端连接
                 * - 成功返回新的已连接 socket：clnt_sock
                 * - clnt_addr 会被填充为客户端地址信息
                 *
                 * 教学重点：
                 * - serv_sock（监听 socket）只负责接收连接请求
                 * - clnt_sock（已连接 socket）才负责与某个客户端进行数据收发
                 */
                clnt_sock = accept(serv_sock,
                                   (struct sockaddr *)&clnt_addr, &clnt_addr_sz);
                if(clnt_sock == -1)
                    error_handling("accept() error");

                /*
                 * 将“客户端 socket”也加入 epoll 监听集合，关注它的可读事件：
                 * - 这里使用的是默认 LT（水平触发）模式（没有 EPOLLET）
                 * - 阻塞 socket + LT 是最基础、最直观的 epoll 示例写法
                 *
                 * LT 的直观理解：
                 * - 只要 socket 接收缓冲区里还有数据没读完，epoll_wait 可能持续返回该 fd 可读
                 * - 因此即使一次 read 没读完，下次还会继续收到通知（相对不容易“丢事件”）
                 */
                event.events = EPOLLIN;
                event.data.fd = clnt_sock;
                epoll_ctl(epfd, EPOLL_CTL_ADD, clnt_sock, &event);
                printf("Connected client: %d\n", clnt_sock);
            }
            else
            {
                /*
                 * 否则就绪的是某个“客户端 socket”
                 * 说明该客户端：
                 * - 有数据可读（EPOLLIN）
                 * 或
                 * - 对端关闭连接，导致 read 返回 0（EOF）
                 */
                str_len = read(ep_events[i].data.fd, buf, BUF_SIZE);

                /*
                 * read 返回值含义（教学重点）：
                 * - >0：成功读取到 str_len 个字节
                 * -  0：对端关闭连接（EOF）
                 * - <0：发生错误（本示例未处理 <0 的情况，工程中应处理）
                 */
                if(str_len == 0)
                {
                    /*
                     * 对端断开：需要清理该连接资源
                     * 1) 从 epoll 集合删除该 fd
                     * 2) close 关闭 socket
                     */
                    epoll_ctl(epfd, EPOLL_CTL_DEL, ep_events[i].data.fd, NULL);
                    close(ep_events[i].data.fd);
                    printf("Closed client: %d\n", ep_events[i].data.fd);
                }
                else
                {
                    /*
                     * 回显（echo）逻辑：
                     * - 把刚读到的 str_len 字节原样写回给客户端
                     *
                     * 教学提示：
                     * - write 也可能出现“部分写”（返回值 < str_len），
                     *   但在阻塞 socket 且数据量不大时通常会写完
                     * - 本示例未检查 write 返回值，作为教学简化
                     */
                    write(ep_events[i].data.fd, buf, str_len);
                }
            }
        }
    }

    /* -------------------- 第四部分：资源释放 -------------------- */

    /*
     * 关闭监听 socket 与 epoll fd
     * 教学提示：
     * - ep_events 由 malloc 分配，严格来说应 free(ep_events)
     * - 进程退出时 OS 会回收内存，但建议养成显式释放的习惯
     */
    close(serv_sock);
    close(epfd);

    return 0;
}

/*
 * error_handling：统一错误处理函数
 * - 将错误信息输出到 stderr（标准错误）
 * - 输出换行
 * - 退出程序
 *
 * 教学补充：
 * - 这里只打印固定字符串，没有输出 errno 对应的具体原因
 * - 更完整的写法常配合 perror 或 strerror(errno)
 */
void error_handling(char *buf)
{
	fputs(buf, stderr);
	fputc('\n', stderr);
	exit(1);
}
