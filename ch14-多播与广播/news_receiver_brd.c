#include <stdio.h>       // 标准输入输出：printf / fputs / fputc 等
#include <errno.h>       // errno：保存最近一次系统调用/库函数失败原因的错误码
#include <stdlib.h>      // 标准库：exit / atoi 等
#include <string.h>      // 内存操作：memset 等
#include <unistd.h>      // POSIX：close 等
 #include <fcntl.h>      // fcntl：获取/设置文件描述符标志；O_NONBLOCK 非阻塞标志
#include <arpa/inet.h>   // 网络字节序转换：htonl / htons 等
#include <sys/socket.h>  // 套接字 API：socket / bind / recvfrom 等

#define BUF_SIZE 30                 // 接收缓冲区大小（每次最多读 BUF_SIZE-1，并手动补 '\0'）
void error_handling(char *message); // 错误处理函数：打印错误信息并退出
void setnonblockingmode(int fd);    // 将指定 fd 设置为非阻塞模式（O_NONBLOCK）

int main(int argc, char* argv[])
{
    int recv_sock;                  // UDP 套接字（用于接收数据报）
    struct sockaddr_in addr;        // 本地地址结构体（绑定用）
    int str_len;                    // recvfrom 返回的接收字节数（或 -1 表示失败）
    char buf[BUF_SIZE];             // 接收缓冲区

    // -------------------- 参数检查 --------------------
    // 本程序作为 UDP 接收端，需要一个参数：绑定端口
    if(argc != 2)
    {
        printf("Usage: %s <port>\n", argv[0]);
        exit(1);
    }

    // -------------------- 创建 UDP 套接字 --------------------
    // PF_INET    : IPv4
    // SOCK_DGRAM : UDP（无连接的数据报）
    // 0          : 自动选择 UDP 协议（IPPROTO_UDP）
    recv_sock = socket(PF_INET, SOCK_DGRAM, 0);
    if(recv_sock == -1)
        error_handling("socket() error");

    // -------------------- 填充并绑定本地地址 --------------------
    memset(&addr, 0, sizeof(addr));      // 清零结构体
    addr.sin_family = AF_INET;           // IPv4
    addr.sin_addr.s_addr = htonl(INADDR_ANY); // 绑定所有本地网卡地址（0.0.0.0）
    addr.sin_port = htons(atoi(argv[1]));     // 端口转网络字节序

    // bind：把 recv_sock 绑定到指定端口
    if(bind(recv_sock, (struct sockaddr*)&addr, sizeof(addr)) == -1)
        error_handling("bind() error");

    // -------------------- 将 socket 设置为非阻塞模式 --------------------
    // 非阻塞含义：
    // - 如果当前没有数据可读，recvfrom 不会阻塞等待；
    // - 而是立即返回 -1，并把 errno 设为 EAGAIN 或 EWOULDBLOCK（表示“暂时没有数据”）。
    setnonblockingmode(recv_sock);

    // -------------------- 接收循环：演示非阻塞 recvfrom 的返回情况 --------------------
    while(1)
    {
        // recvfrom：尝试接收 UDP 数据报
        // 参数最后的 NULL,0 表示不关心发送方地址
        //
        // 在非阻塞模式下：
        // - 若有数据：返回 >0 的字节数
        // - 若无数据：立即返回 -1，errno=EAGAIN/EWOULDBLOCK
        // - 其它错误：返回 -1，errno 为对应错误码
        str_len = recvfrom(recv_sock, buf, BUF_SIZE - 1, 0,
            NULL, 0);

        // 打印 errno 与 str_len，用于观察非阻塞行为
        // 注意：
        // - errno 只有在调用返回 -1 时才有意义；
        // - 如果 recvfrom 成功，errno 可能仍保留上一次失败的值（不会自动清零）
        // 因此这里的 printf("%d\n", errno) 主要是教学演示用法。
        printf("%d\n", errno);
        printf("str_len: %d\n", str_len);

        // 只要 recvfrom 返回负数就 break：
        // 这意味着程序在“第一次无数据时”（EAGAIN/EWOULDBLOCK）就会退出循环，
        // 所以该示例更像是用来演示：非阻塞时会立刻返回 -1。
        //
        // 如果想让程序持续运行并等待数据，通常会：
        // - 若 errno==EAGAIN/EWOULDBLOCK，则 continue（并可 sleep/usleep 避免忙等）
        // - 或使用 select/poll/epoll 等等待可读事件
        if(str_len < 0)
            break;

        // 成功接收后，把数据当作字符串输出：
        // 先补 '\0'
        buf[str_len] = 0;

        // 输出到标准输出
        fputs(buf, stdout);
    }

    // -------------------- 关闭 socket --------------------
    close(recv_sock);
    return 0;
}

void error_handling(char *message)
{
    // 输出错误信息到标准错误流 stderr
	fputs(message, stderr);
	fputc('\n', stderr);
	exit(1); // 非正常退出
}

void setnonblockingmode(int fd)
{
    // fcntl(fd, F_GETFL, 0)：获取该文件描述符当前的文件状态标志（例如 O_NONBLOCK 等）
	int flag=fcntl(fd, F_GETFL, 0);

    // fcntl(fd, F_SETFL, flag | O_NONBLOCK)：在原有标志基础上追加 O_NONBLOCK
    // 这样 fd 就变为“非阻塞模式”
	fcntl(fd, F_SETFL, flag|O_NONBLOCK);
}
