#include <stdio.h>      // 标准输入输出：printf / fputs / fputc 等
#include <unistd.h>     // POSIX：write / close 等
#include <stdlib.h>     // 标准库：exit / atoi 等
#include <string.h>     // 字符串/内存操作：memset / strlen 等
#include <sys/socket.h> // 套接字 API：socket / connect / send 等，以及 MSG_OOB 等标志
#include <arpa/inet.h>  // 网络地址转换/字节序：inet_addr / htons 等

#define BUF_SIZE 30                 // 缓冲区大小（本文件未直接使用，但与配套示例保持一致）
void error_handling(char *message); // 错误处理函数：输出错误信息并退出

int main(int argc, char *argv[])
{
    int sock;                       // 客户端 TCP 套接字
    struct sockaddr_in recv_adr;    // 服务器地址结构体（IPv4）

    // -------------------- 参数检查 --------------------
    // 需要两个参数：服务器 IP 与端口
    // argv[1] = IP（例如 127.0.0.1）
    // argv[2] = port（例如 9190）
    if (argc != 3)
    {
        printf("Usage : %s <IP> <port>\n", argv[0]);
        exit(1);
    }

    // -------------------- 创建 TCP 套接字 --------------------
    // PF_INET     : IPv4
    // SOCK_STREAM : TCP（面向连接、字节流）
    // 0           : 自动选择 TCP 协议（IPPROTO_TCP）
    sock = socket(PF_INET, SOCK_STREAM, 0);

    // -------------------- 填充服务器地址 --------------------
    memset(&recv_adr, 0, sizeof(recv_adr));      // 清零，避免未初始化字段
    recv_adr.sin_family = AF_INET;               // IPv4
    recv_adr.sin_addr.s_addr = inet_addr(argv[1]); // 点分十进制 IP 字符串 -> 网络字节序 IPv4 地址
    recv_adr.sin_port = htons(atoi(argv[2]));    // 端口：atoi 转 int，htons 转网络字节序

    // -------------------- 发起连接 --------------------
    // connect：与服务器建立 TCP 连接（三次握手）
    if (connect(sock, (struct sockaddr *)&recv_adr, sizeof(recv_adr)) == -1)
        error_handling("connect() error!");

    // -------------------- 发送普通数据（in-band data） --------------------
    // write：向已连接 TCP 套接字发送普通字节流数据
    // 这里发送 "123"（不包含 '\0'）
    write(sock, "123", strlen("123"));

    // -------------------- 发送紧急数据（out-of-band / OOB） --------------------
    // send(..., MSG_OOB)：发送“紧急数据”
    // MSG_OOB 会把本次发送的数据标记为 TCP 的紧急数据（urgent）
    // 配套服务器端若设置了 SIGURG 处理并用 recv(..., MSG_OOB) 接收，就能以“紧急消息”的方式读到
    //
    // 重要说明（概念层面）：
    // - TCP 的 OOB 并不是完全独立的第二条通道；它通过“紧急指针”机制标记紧急位置
    // - 许多系统实现中，紧急数据通常体现为“紧急指针之前的最后一个字节”
    // - 教学示例通常用它配合 SIGURG 展示紧急消息处理流程
    send(sock, "4", strlen("4"), MSG_OOB);

    // 再发送一段普通数据 "567"
    write(sock, "567", strlen("567"));

    // 再发送一段“紧急数据” "890"
    // 注意：某些实现中多字节 OOB 的行为可能和预期不同（可能只把最后一个字节视为紧急字节），
    // 但示例用于演示 MSG_OOB 与接收端 recv(MSG_OOB)/SIGURG 的搭配
    send(sock, "890", strlen("890"), MSG_OOB);

    // -------------------- 关闭连接 --------------------
    close(sock);
    return 0;
}

void error_handling(char *message)
{
    // 输出错误信息到标准错误流 stderr
    fputs(message, stderr);
    fputc('\n', stderr);
    exit(1); // 非正常退出
}
