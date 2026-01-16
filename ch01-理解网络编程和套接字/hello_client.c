#include <stdio.h>      // printf, fputs, stderr
#include <stdlib.h>     // exit, atoi
#include <string.h>     // memset
#include <unistd.h>     // read, close
#include <arpa/inet.h>  // inet_addr, htons, sockaddr_in
#include <sys/socket.h> // socket, connect

// 统一的错误处理：输出错误信息并终止程序
void error_handing(char *message);

int main(int argc, char* argv[])
{
    int sock;                     // 客户端套接字描述符
    struct sockaddr_in serv_addr; // 服务器地址信息
    char message[30];             // 接收缓冲区
    int str_len;                  // 从服务器读取的字节数

    // 参数校验：需要传入服务器 IP 和端口号
    if(argc != 3)
    {
        printf("Usage: %s <IP> <port>\n", argv[0]);
        exit(1);
    }

    // 创建 TCP 套接字
    sock = socket(PF_INET, SOCK_STREAM, 0);
    if(sock == -1)
        error_handing("socket() error");

    // 初始化服务器地址结构体（IPv4）
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    // 将点分十进制 IP 转为网络字节序
    serv_addr.sin_addr.s_addr = inet_addr(argv[1]);
    // 端口号转换为网络字节序
    serv_addr.sin_port = htons(atoi(argv[2]));

    // 连接到服务器（阻塞直到连接成功或失败）
    if(connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == -1)
        error_handing("connet() error");

    // 从服务器读取数据；预留 1 字节给字符串结束符
    str_len = read(sock, message, sizeof(message) - 1);
    if(str_len == -1)
        error_handing("read() error!");

    // 本例服务器发送的是以 '\0' 结尾的字符串，可直接打印
    printf("Message from server: %s\n", message);
    // 关闭套接字，结束客户端程序
    close(sock);

    return 0;
}

void error_handing(char *message)
{
    // 将错误信息输出到标准错误并终止程序
    fputs(message, stderr);
    fputc('\n', stderr);
    exit(1);
}
