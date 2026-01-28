#include <stdio.h>      // 标准输入输出：printf / fputs / fputc 等
#include <unistd.h>     // POSIX：close 等
#include <stdlib.h>     // 标准库：exit / atoi 等
#include <string.h>     // 内存操作：memset 等
#include <sys/socket.h> // 套接字 API：socket / bind / listen / accept / recv 等，以及 MSG_PEEK/MSG_DONTWAIT 标志
#include <arpa/inet.h>  // 网络字节序转换：htonl / htons 等

#define BUF_SIZE 30                 // 接收缓冲区大小（示例中最多读 BUF_SIZE-1 并手动补 '\0'）
void error_handling(char *message); // 错误处理函数：输出错误并退出

int main(int argc, char* argv[])
{
    int serv_sock, clnt_sock;                // serv_sock：监听套接字；clnt_sock：与客户端建立连接后的套接字
    struct sockaddr_in serv_addr, clnt_addr; // serv_addr：服务器地址；clnt_addr：客户端地址（accept 填充）
    socklen_t clnt_addr_sz;                  // accept 用：客户端地址结构体长度

    int str_len;                             // recv 返回的字节数
    char buf[BUF_SIZE];                      // 接收缓冲区

    // -------------------- 参数检查 --------------------
    // 服务器只需要一个参数：监听端口
    if(argc != 2)
    {
        printf("Usage: %s <port>\n", argv[0]);
        exit(1);
    }

    // -------------------- 创建 TCP 监听套接字 --------------------
    // PF_INET     : IPv4
    // SOCK_STREAM : TCP（面向连接）
    // 0           : 自动选择 TCP 协议（IPPROTO_TCP）
    serv_sock = socket(PF_INET, SOCK_STREAM, 0);

    // -------------------- 填充并绑定服务器地址 --------------------
    memset(&serv_addr, 0, sizeof(serv_addr));      // 清零结构体
    serv_addr.sin_family = AF_INET;                // IPv4
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY); // 绑定所有网卡地址（0.0.0.0）
    serv_addr.sin_port = htons(atoi(argv[1]));     // 端口转网络字节序

    // bind：绑定端口
    if(bind(serv_sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == -1)
        error_handling("bind() error");

    // listen：开始监听连接，backlog=5（等待队列上限，实现相关）
    if(listen(serv_sock, 5) == -1)
        error_handling("listen() error");

    // accept：等待客户端连接，成功后返回已连接套接字 clnt_sock
    clnt_addr_sz = sizeof(clnt_addr);
    clnt_sock = accept(serv_sock, (struct sockaddr*)&clnt_addr, &clnt_addr_sz);
    // 注意：这里未检查 accept 是否返回 -1，真实工程应做错误处理

    // -------------------- “窥探”(peek) 接收缓冲区：MSG_PEEK --------------------
    // 目标：演示 MSG_PEEK 的效果——读取数据但不从接收缓冲区移除数据。
    //
    // MSG_DONTWAIT：让 recv 以非阻塞方式执行：
    // - 如果当前没有数据可读，recv 会立即返回 -1，并设置 errno = EAGAIN/EWOULDBLOCK
    //
    // MSG_PEEK：窥探数据：
    // - recv 会把数据拷贝到用户缓冲区 buf
    // - 但不会把这些数据从 socket 的接收缓冲区“取走”
    // - 因此随后再次 recv（不带 MSG_PEEK）仍然可以读到同样的数据
    //
    // 这里使用 while(1) 轮询：不断尝试 peek，直到读到 >0 字节为止
    while(1)
    {
        // 最多读取 BUF_SIZE-1 字节，为后续补 '\0' 预留空间
        // 返回值 str_len：
        // - >0：读到的字节数（peek 到的数据长度）
        // - 0 ：对端关闭连接（EOF）——本示例未处理这种情况，可能导致死循环
        // - -1：出错或无数据（配合 MSG_DONTWAIT 时，常见是暂时无数据）
        str_len = recv(clnt_sock, buf, BUF_SIZE - 1, MSG_PEEK|MSG_DONTWAIT);

        // 一旦 peek 到数据（str_len > 0），跳出循环
        if(str_len > 0)
            break;

        // 若 str_len == -1 且无数据，会在这里继续忙等（占 CPU）
        // 教学示例为了突出 MSG_PEEK/MSG_DONTWAIT 的效果，省略了 sleep/usleep。
    }

    // 将 peek 到的数据当作字符串输出，需要补 '\0'
    buf[str_len] = 0;

    // 输出：表明当前从 socket 接收缓冲区“窥探”到了多少字节，以及内容是什么
    printf("Buffering %d bytes: %s\n", str_len, buf);

    // -------------------- 再次 recv（不带 MSG_PEEK）：真正取走数据 --------------------
    // 因为刚才用 MSG_PEEK 并没有把数据从接收缓冲区移除，
    // 所以下面这次 recv 仍然会读到同样的数据（至少在数据未被其他读取操作取走的情况下）。
    str_len = recv(clnt_sock, buf, BUF_SIZE - 1, 0);

    // 同样补 '\0' 并输出
    buf[str_len] = 0;
    printf("Read again: %s\n", buf);

    // -------------------- 关闭套接字 --------------------
    close(serv_sock);
    close(clnt_sock);
    return 0;
}

void error_handling(char *message)
{
    // 输出错误信息到标准错误流 stderr
	fputs(message, stderr);
	fputc('\n', stderr);
	exit(1); // 非正常退出
}
