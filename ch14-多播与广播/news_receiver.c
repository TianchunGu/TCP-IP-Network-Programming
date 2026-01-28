#include <stdio.h>      // 标准输入输出：printf / fputs / fputc 等
#include <stdlib.h>     // 标准库：exit / atoi 等
#include <string.h>     // 内存操作：memset 等
#include <unistd.h>     // POSIX：close 等
#include <arpa/inet.h>  // 网络字节序转换：htonl / htons 等
#include <sys/socket.h> // 套接字 API：socket / bind / recvfrom 等

#define BUF_SIZE 30                 // 接收缓冲区大小（每次最多读取 BUF_SIZE-1 并手动补 '\0'）
void error_handling(char *message); // 错误处理函数：打印错误信息并退出

int main(int argc, char *argv[])
{
    int recv_sock;                  // UDP 套接字（用于接收数据）
    struct sockaddr_in adr;         // 本地地址结构体（服务器端绑定用）
    int str_len;                    // recvfrom 返回的实际接收字节数
    char buf[BUF_SIZE];             // 接收缓冲区

    // -------------------- 参数检查 --------------------
    // 本程序作为 UDP 接收端，只需要一个参数：绑定端口
    // argv[1] = PORT（例如 9190）
    if (argc != 2)
    {
        printf("Usage : %s  <PORT>\n", argv[0]);
        exit(1);
    }

    // -------------------- 创建 UDP 套接字 --------------------
    // PF_INET    : IPv4
    // SOCK_DGRAM : UDP（无连接的数据报）
    // 0          : 自动选择 UDP 协议（IPPROTO_UDP）
    recv_sock = socket(PF_INET, SOCK_DGRAM, 0);

    // -------------------- 填充并绑定本地地址 --------------------
    memset(&adr, 0, sizeof(adr));      // 清零结构体，避免未初始化字段
    adr.sin_family = AF_INET;          // IPv4
    adr.sin_addr.s_addr = htonl(INADDR_ANY); // 绑定到所有本地网卡地址（0.0.0.0）
    adr.sin_port = htons(atoi(argv[1]));     // 端口：atoi 转 int，再 htons 转网络字节序

    // bind：把 recv_sock 绑定到本地指定端口
    // 绑定后，发送到该端口的 UDP 数据报就会被该 socket 接收
    if (bind(recv_sock, (struct sockaddr *)&adr, sizeof(adr)) == -1)
        error_handling("bind() error");

    // -------------------- 接收循环：不断接收 UDP 数据报并输出 --------------------
    while (1)
    {
        // recvfrom：接收 UDP 数据报
        // 参数说明：
        // recv_sock         : 目标 socket
        // buf               : 接收缓冲区
        // BUF_SIZE - 1      : 最多接收 BUF_SIZE-1 字节，预留 1 字节用于补 '\0'
        // 0                 : 标志位（此处不设置特殊标志）
        // NULL, 0           : 不关心发送方地址（因此不保存对端 IP/端口）
        //
        // 返回值 str_len：
        // - >0：实际收到的字节数
        // -  0：理论上 UDP 也可能收到长度为 0 的数据报（合法但少见）
        // - <0：出错（例如被信号中断等）
        str_len = recvfrom(recv_sock, buf, BUF_SIZE - 1, 0, NULL, 0);

        // 若接收出错（返回负数），跳出循环并结束程序
        if (str_len < 0)
            break;

        // 将接收到的数据当作 C 字符串输出时，手动添加结束符
        buf[str_len] = 0;

        // fputs：把字符串输出到 stdout（不会自动追加换行）
        // 是否换行取决于发送端发送的数据是否包含 '\n'
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
