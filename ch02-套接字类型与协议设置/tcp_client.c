#include <stdio.h>      // printf, fputs, stderr
#include <stdlib.h>     // exit, atoi
#include <string.h>     // memset
#include <unistd.h>     // read, close
#include <arpa/inet.h>  // inet_addr, htons, sockaddr_in
#include <sys/types.h>  // 基本系统数据类型
#include <sys/socket.h> // socket, connect

// 统一错误处理：输出错误信息并终止程序
void error_handling(char *message);

int main(int argc, char* argv[])
{
    int sock;                     // 客户端套接字描述符
    struct sockaddr_in serv_addr; // 服务器地址
    char message[30];             // 接收缓冲区
    int str_len = 0;              // 累计读取字节数
    int idx = 0, read_len = 0;    // 当前写入位置与单次读取字节数

    // 参数校验：需要服务器 IP 和端口号
    if(argc != 3)
    {
        printf("Usage: %s <IP> <port>", argv[0]);
        exit(1);
    }

    // 创建 TCP 套接字
    sock = socket(PF_INET, SOCK_STREAM, 0);
    if(sock == -1)
        error_handling("socket() error");

    // 初始化服务器地址结构体
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    // 将点分十进制 IP 转为网络字节序
    serv_addr.sin_addr.s_addr = inet_addr(argv[1]);
    // 端口号转换为网络字节序
    serv_addr.sin_port = htons(atoi(argv[2]));

    // 连接到服务器
    if(connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == -1)
        error_handling("connect() error");

    // 逐字节读取服务器发送的数据，直到对端关闭连接
    while(read_len = read(sock, &message[idx++], 1))
    {
        if(read_len == -1)
            error_handling("read() error");

        // 累计读取字节数（用于统计 read 调用次数）
        str_len += read_len;
    }

    // 输出收到的消息与 read 调用次数
    printf("Message from server: %s\n", message);
    printf("Function read call count: %d\n", str_len);
    // 关闭套接字
    close(sock);

    return 0;
}

void error_handling(char *message)
{
    // 将错误信息输出到标准错误并终止程序
    fputs(message, stderr);
    fputc('\n', stderr);
    exit(1);
}
