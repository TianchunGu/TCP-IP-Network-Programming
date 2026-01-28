#include <stdio.h>      // 标准输入输出：printf / fputs / fputc
#include <unistd.h>     // POSIX：write / close
#include <stdlib.h>     // 标准库：exit / atoi
#include <string.h>     // 内存/字符串操作：memset
#include <sys/socket.h> // 套接字 API：socket / connect
#include <arpa/inet.h>  // 网络地址与字节序：inet_addr / htons
void error_handling(char *message); // 错误处理函数声明：打印错误信息并退出

int main(int argc, char* argv[])
{
    int sock;                        // 客户端套接字文件描述符（用于 TCP 连接）
    struct sockaddr_in send_addr;    // 服务器地址结构体（IPv4）

    // -------------------- 参数检查 --------------------
    // 程序需要两个参数：服务器 IP 与端口
    // argv[1] = IP（例如 127.0.0.1）
    // argv[2] = port（例如 9190）
    if(argc != 3)
    {
        printf("Usage: %s <IP> <port>\n", argv[0]);
        exit(1); // 参数不正确则退出
    }

    // -------------------- 创建 TCP 套接字 --------------------
    // PF_INET     : IPv4
    // SOCK_STREAM : TCP（面向连接的字节流）
    // 0           : 自动选择 TCP 协议（IPPROTO_TCP）
    sock = socket(PF_INET, SOCK_STREAM, 0);

    // -------------------- 填充服务器地址 --------------------
    memset(&send_addr, 0, sizeof(send_addr));       // 清零结构体，避免未初始化字段
    send_addr.sin_family = AF_INET;                 // IPv4
    send_addr.sin_addr.s_addr = inet_addr(argv[1]); // 点分十进制 IP -> 网络字节序 IPv4 地址
    send_addr.sin_port = htons(atoi(argv[2]));      // 端口：atoi 转 int，htons 转网络字节序

    // -------------------- 连接服务器 --------------------
    // connect：触发 TCP 三次握手，成功后 sock 成为“已连接套接字”
    if(connect(sock, (struct sockaddr*)&send_addr, sizeof(send_addr)) == -1)
        error_handling("connect() error");

    // -------------------- 发送数据 --------------------
    // write(sock, "123", sizeof("123"))：
    // - 向 TCP 连接写入数据
    // - 注意 sizeof("123") 的结果是 4，而不是 3：
    //   因为字符串字面量 "123" 包含结尾的 '\0'
    //   即实际写入的字节序列为：'1' '2' '3' '\0'
    //
    // 对比：若用 strlen("123") 则只发送 3 个字符，不包含 '\0'
    write(sock, "123", sizeof("123"));

    // -------------------- 关闭套接字 --------------------
    // close 会关闭文件描述符，并触发 TCP 连接关闭流程（发送 FIN 等）
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
