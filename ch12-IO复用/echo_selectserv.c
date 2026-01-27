#include <stdio.h>       // 标准输入输出：printf / fputs / fputc 等
#include <stdlib.h>      // 标准库：exit / atoi 等
#include <string.h>      // 内存操作：memset 等
#include <unistd.h>      // POSIX：read / write / close 等
#include <arpa/inet.h>   // 网络字节序转换：htonl / htons 等
#include <sys/socket.h>  // 套接字 API：socket / bind / listen / accept 等
#include <sys/time.h>    // 时间结构：struct timeval
#include <sys/select.h>  // I/O 多路复用：select / fd_set / FD_* 宏

#define BUF_SIZE 100                 // 网络收发缓冲区大小：每次读/写最多 100 字节
void error_handling(char *buf);      // 错误处理函数：打印错误信息并退出

int main(int argc, char* argv[])
{
    int serv_sock, clnt_sock;                 // serv_sock：监听套接字；clnt_sock：accept 返回的客户端连接套接字
    struct sockaddr_in serv_addr, clnt_addr;  // serv_addr：服务器地址；clnt_addr：客户端地址（accept 填充）
    socklen_t addr_sz;                        // accept 的地址长度参数

    struct timeval timeout;                   // select 的超时结构（秒 + 微秒）
    fd_set reads, cpy_reads;                  // reads：监视集合；cpy_reads：select 调用时使用的副本集合

    int str_len, fd_num, fd_max;              // str_len：read 返回的字节数
                                              // fd_num：select 返回的“就绪 fd 数量”
                                              // fd_max：当前监视的最大 fd 值（用于 nfds=fd_max+1）
    char buf[BUF_SIZE];                       // 读写缓冲区（回显服务器）

    // -------------------- 参数检查 --------------------
    // 服务器程序只需要一个参数：监听端口
    if(argc != 2)
    {
        printf("Usage: %s <port>\n", argv[0]);
        exit(1);
    }

    // -------------------- 创建监听 socket 并绑定 --------------------
    // PF_INET     : IPv4
    // SOCK_STREAM : TCP（面向连接）
    // 0           : 自动选择 TCP 协议（IPPROTO_TCP）
    serv_sock = socket(PF_INET, SOCK_STREAM, 0);

    // 初始化服务器地址结构体
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;                // IPv4
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY); // 绑定所有网卡地址（0.0.0.0）
    serv_addr.sin_port = htons(atoi(argv[1]));     // 端口转网络字节序

    // 绑定端口：让该 TCP socket 在指定端口上监听连接
    if(bind(serv_sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == -1)
        error_handling("bind() error");

    // 进入监听状态：backlog=5 表示等待队列上限（实现相关）
    if(listen(serv_sock, 5) == -1)
        error_handling("listen() error");

    // -------------------- 初始化 select 监视集合 --------------------
    // FD_ZERO：清空集合
    FD_ZERO(&reads);

    // 将监听套接字加入监视集合：
    // 当 serv_sock 可读时，通常意味着“有新连接到来”，可以 accept
    FD_SET(serv_sock, &reads);

    // 记录当前最大 fd 值（后续 select 的 nfds 参数需要 fd_max+1）
    fd_max = serv_sock;

    // -------------------- 主循环：使用 select 实现 I/O 多路复用 --------------------
    while(1)
    {
        // 每轮都设置超时时间（select 返回后 timeout 可能被修改为剩余时间，因此需要重置）
        timeout.tv_sec = 5;       // 5 秒
        timeout.tv_usec = 5000;   // 5000 微秒 = 0.005 秒（总计约 5.005 秒）

        // select 会“原地修改”传入的 fd_set：
        // - 返回时只保留就绪的 fd，其它位会被清掉
        // 因此必须把 reads 复制一份给 select 使用
        cpy_reads = reads;

        // select(nfds, &readfds, &writefds, &exceptfds, &timeout)
        // nfds = fd_max + 1：必须大于集合中最大 fd 的值
        // 这里只监视读事件（&cpy_reads），写事件和异常事件不监视（传 0）
        if((fd_num = select(fd_max + 1, &cpy_reads, 0, 0, &timeout)) == -1)
            break; // select 出错则退出循环

        // fd_num == 0 表示超时（timeout 时间内没有任何 fd 就绪）
        if(fd_num == 0)
            continue;

        // 遍历 0..fd_max，找出哪些 fd 在 cpy_reads 中被标记为“可读”
        // 注意：select 返回就绪数量 fd_num，但这里仍采用遍历的方式逐个检查 FD_ISSET
        for(int i = 0; i < fd_max + 1; i++)
        {
            // FD_ISSET(i, &cpy_reads)：判断 fd=i 是否就绪（可读）
            if(FD_ISSET(i, &cpy_reads))
            {
                if(i == serv_sock)//i是服务器监听套接字，表示有新客户端来请求连接了
                {
                    // -------------------- 监听套接字就绪：有新连接到来 --------------------
                    addr_sz = sizeof(clnt_addr);

                    // accept：从监听队列取出一个已完成握手的连接，返回新的“已连接套接字” clnt_sock
                    clnt_sock = accept(serv_sock,
                        (struct sockaddr*)&clnt_addr, &addr_sz);

                    // 将新的客户端 socket 加入监视集合，之后就可以通过 select 监听它的可读事件
                    FD_SET(clnt_sock, &reads);

                    // 更新最大 fd 值，保证 select 的 nfds 参数覆盖到最新的 socket
                    if(fd_max < clnt_sock)
                        fd_max = clnt_sock;

                    // 打印该客户端连接对应的文件描述符（便于观察）
                    printf("Connected client: %d\n", clnt_sock);
                }
                else//是客户端套接字 (i != serv_sock)，已经连接的客户端发消息来了。
                {
                    // -------------------- 客户端套接字就绪：可读（有数据到达或对端关闭） --------------------
                    // 从该客户端读取数据（最多 BUF_SIZE 字节）
                    // 返回值 str_len：
                    // - >0：读取到的字节数
                    // - 0 ：对端关闭连接（EOF）
                    // - -1：出错（本示例未单独处理 -1）
                    str_len = read(i, buf, BUF_SIZE);

                    if(str_len == 0)
                    {
                        // 对端关闭连接：从监视集合中移除该 fd，并关闭它
                        FD_CLR(i, &reads);
                        close(i);
                        printf("Closed client: %d\n", i);
                    }
                    else
                    {
                        // 回显：把读到的数据原样写回给同一个客户端
                        // 注意：TCP 是字节流协议；示例中假设 write 能一次写完 str_len 字节（教学简化）
                        write(i, buf, str_len);
                    }
                }
            }
        }
    }

    // 关闭监听套接字（释放资源）
    close(serv_sock);
    return 0;
}

void error_handling(char *buf)
{
    // 将错误信息输出到标准错误流 stderr
	fputs(buf, stderr);
	fputc('\n', stderr);
	exit(1); // 非正常退出
}
