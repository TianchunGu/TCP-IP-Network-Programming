#include <stdio.h>      // printf, puts, fputs, stderr 等标准I/O函数
#include <stdlib.h>     // exit, malloc, atoi
#include <string.h>     // memset
#include <unistd.h>     // read, write, close（POSIX I/O 系统调用）
#include <fcntl.h>      // （本程序未使用 fcntl，但保留不改动）
#include <arpa/inet.h>  // htonl, htons：网络字节序转换
#include <sys/socket.h> // socket, bind, listen, accept：套接字系统调用
#include <sys/epoll.h>  // epoll_create, epoll_ctl, epoll_wait, epoll_event

#define BUF_SIZE 4      // 读写缓冲区大小（故意很小，便于观察“分块读/写”的效果）
#define EPOLL_SIZE 50   // epoll 相关数组容量：一次最多处理 50 个就绪事件
void error_handling(char *buf);

int main(int argc, char* argv[])
{
    int serv_sock, clnt_sock;
    struct sockaddr_in serv_addr, clnt_addr;
    socklen_t clnt_addr_sz;
    int str_len;
    char buf[BUF_SIZE];

    struct epoll_event *ep_events;
    struct epoll_event event;
    int epfd, event_cnt;

    /*
     * 参数检查：
     * 期望命令行只提供一个参数：监听端口
     * argv[0]：程序名
     * argv[1]：端口号字符串，例如 "8080"
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
     * 0：自动选择协议（通常就是 TCP）
     */
    serv_sock = socket(PF_INET, SOCK_STREAM, 0);
    if(serv_sock == -1)
        error_handling("socket() error");

    /*
     * 2) 填写服务器地址结构体 serv_addr
     * memset 清零避免未初始化字段造成不可预期行为
     */
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;              // IPv4
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    /*
     * INADDR_ANY：0.0.0.0，表示绑定本机所有网卡/所有 IP
     * htonl：host to network long，把 32 位整数转为网络字节序（大端）
     */
    serv_addr.sin_port = htons(atoi(argv[1]));
    /*
     * atoi(argv[1])：端口字符串 -> 整数（主机字节序）
     * htons：host to network short，把 16 位端口转为网络字节序（大端）
     */

    /*
     * 3) bind：把监听 socket 与本地 IP:端口绑定
     * bind 失败常见原因：
     * - 端口已被占用（Address already in use）
     * - 权限不足（绑定小于 1024 的端口通常需要管理员权限）
     */
    if(bind(serv_sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == -1)
        error_handling("bind() error");

    /*
     * 4) listen：进入监听状态
     * backlog=5：等待 accept 的连接请求队列长度上限
     */
    if(listen(serv_sock, 5) == -1)
        error_handling("listen() error");

    /*
     * 5) 创建 epoll 实例
     * epoll_create(EPOLL_SIZE)：
     * - 返回 epoll 的文件描述符 epfd
     * - 参数 EPOLL_SIZE 在现代内核中更多是“历史遗留提示值”，但必须 >0
     */
    epfd = epoll_create(EPOLL_SIZE);

    /*
     * 为 epoll_wait 分配就绪事件数组：
     * - epoll_wait 会把就绪事件填充到 ep_events 中
     * - 容量为 EPOLL_SIZE，表示一次最多处理这么多就绪事件
     */
    ep_events = malloc(sizeof(struct epoll_event) * EPOLL_SIZE);

    /*
     * 6) 将监听 socket（serv_sock）加入 epoll 关注列表
     *
     * event.events = EPOLLIN：
     * - EPOLLIN 表示“可读事件”
     * - 对监听 socket 来说，“可读”通常意味着：有新的连接请求到达，可以 accept
     *
     * event.data.fd = serv_sock：
     * - 让 epoll 在返回事件时告诉我们：哪个 fd 就绪了
     */
    event.events = EPOLLIN;
    event.data.fd = serv_sock;

    /*
     * epoll_ctl(epfd, EPOLL_CTL_ADD, serv_sock, &event)
     * - 把 serv_sock 添加到 epoll 监听集合
     */
    epoll_ctl(epfd, EPOLL_CTL_ADD, serv_sock, &event);

    while(1)
    {
        /*
         * 7) 等待事件发生
         * epoll_wait(epfd, ep_events, EPOLL_SIZE, -1)
         * - -1 表示一直阻塞，直到至少有一个 fd 就绪
         *
         * 返回值 event_cnt：
         * - >0：返回的就绪事件数量
         * - 0：超时（本例不会，因为 timeout=-1）
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
         * 8) 逐个处理本次就绪的事件
         */
        for(int i = 0; i < event_cnt; i++)
        {
            /*
             * 如果就绪的是监听 socket：
             * - 说明有新客户端连接到来
             * - 需要 accept() 取出连接，得到新的 clnt_sock
             */
            if(ep_events[i].data.fd == serv_sock)
            {
                clnt_addr_sz = sizeof(clnt_addr);

                /*
                 * accept：接收一个客户端连接
                 * - 成功返回新的“已连接 socket”clnt_sock，用于与该客户端通信
                 * - clnt_addr 会被填充为客户端地址信息
                 *
                 * 教学提示：
                 * - accept 默认是阻塞的，但此处只有在 epoll 提示“可读（有连接）”时才调用，
                 *   所以一般不会无故阻塞很久
                 */
                clnt_sock = accept(serv_sock,
                    (struct sockaddr*)&clnt_addr, &clnt_addr_sz);

                /*
                 * 把新客户端 socket 加入 epoll 监听集合，关注其“可读事件”：
                 * - 该程序采用默认 LT（水平触发）模式：
                 *   只要 socket 接收缓冲区里还有数据没读完，epoll_wait 仍可能继续返回它可读
                 * - 本代码未使用 EPOLLET（边缘触发），也未设置非阻塞模式
                 *   这是 LT + 阻塞 fd 的典型教学示例写法
                 */
                event.events = EPOLLIN;
                event.data.fd = clnt_sock;
                epoll_ctl(epfd, EPOLL_CTL_ADD, clnt_sock, &event);
                printf("Connected client: %d\n", clnt_sock);
            }
            else
            {
                /*
                 * 否则就绪的是某个客户端 socket：
                 * - EPOLLIN 表示该客户端有数据可读，或对方关闭导致读返回 0
                 *
                 * read(fd, buf, BUF_SIZE)：
                 * - 最多读取 BUF_SIZE 个字节到 buf
                 * - 返回值 str_len：
                 *   * >0：读到的字节数
                 *   * 0 ：对端关闭连接（EOF）
                 *   * <0：出错（本代码未处理出错分支）
                 *
                 * 教学提示：
                 * - BUF_SIZE=4 很小，因此客户端发送的较长数据会被分多次读出并回显
                 * - 由于是 LT 模式，就算一次 read 没读完，epoll 还会继续通知
                 */
                str_len = read(ep_events[i].data.fd, buf, BUF_SIZE);

                /*
                 * str_len == 0 表示客户端关闭连接：
                 * - 需要从 epoll 监听集合删除该 fd
                 * - 并 close 释放资源
                 */
                if(str_len == 0)
                {
                    /*
                     * EPOLL_CTL_DEL：从 epoll 监听集合删除该 fd
                     * 第四个参数在 DEL 时可为 NULL
                     */
                    epoll_ctl(epfd, EPOLL_CTL_DEL,
                        ep_events[i].data.fd, NULL);

                    close(ep_events[i].data.fd); // 关闭客户端 socket
                    printf("Closed client: %d\n", ep_events[i].data.fd);
                }
                else
                {
                    /*
                     * 回显（echo）逻辑：
                     * - 把刚读到的 str_len 字节原样写回客户端
                     *
                     * 教学提示：
                     * - write 也可能出现“部分写”（返回值 < str_len），
                     *   但对普通阻塞 socket 且数据量小的示例往往一次写完
                     * - 本代码未检查 write 返回值，作为教学示例简化处理
                     */
                    write(ep_events[i].data.fd, buf, str_len);
                }
            }
        }
    }

    /*
     * 9) 程序结束前关闭监听 socket 与 epoll fd
     * 教学提示：
     * - ep_events 由 malloc 分配，严格来说应 free(ep_events)
     * - 进程退出时 OS 会回收内存，但良好习惯是显式释放
     */
    close(serv_sock);
    close(epfd);
    return 0;
}

/*
 * error_handling：统一错误处理函数
 * - 打印错误信息到标准错误 stderr
 * - 输出换行
 * - 退出程序
 *
 * 教学提示：
 * - 这里只打印自定义字符串，没有输出 errno 对应原因
 * - 更完整的错误处理通常会配合 perror 或 strerror(errno)
 */
void error_handling(char *buf)
{
	fputs(buf, stderr);
	fputc('\n', stderr);
	exit(1);
}
