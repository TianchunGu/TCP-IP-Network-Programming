#include <stdio.h>      // printf, fputs, fgets, stdout
#include <stdlib.h>     // exit, atoi
#include <string.h>     // memset, strcmp, strlen
#include <unistd.h>     // read, write, close
#include <arpa/inet.h>  // inet_addr, htons, sockaddr_in
#include <sys/socket.h> // socket, connect

#define BUF_SIZE 1024 // 输入/输出缓冲区大小
void error_handling(char *message);

int main(int argc, char *argv[])
{
    int sock;                    // 客户端套接字描述符
    struct sockaddr_in serv_addr; // 服务器地址
    char message[BUF_SIZE];      // 发送与接收缓冲区
    int str_len;                 // 实际读取的字节数

    // 参数校验：需要服务器 IP 与端口号
    if(argc != 3)
    {
        printf("Usage: %s <IP> <port>\n", argv[0]);
        exit(1);
    }

    // 创建 TCP 套接字
    sock = socket(PF_INET, SOCK_STREAM, 0);
    if(sock == -1)
        error_handling("socket() error");

    // 初始化服务器地址
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    // IP 字符串转换为网络字节序
    serv_addr.sin_addr.s_addr = inet_addr(argv[1]);
    // 端口转换为网络字节序
    serv_addr.sin_port = htons(atoi(argv[2]));

    // 连接服务器
    if(connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1)
        error_handling("connect() error");
    else
        puts("Connected......");

    // 交互式回显：读一行 -> 发给服务器 -> 接收并打印
    while (1)
    {
        fputs("Input Message(q/Q to quit): ", stdout);
        fgets(message, BUF_SIZE, stdin);

        // 输入 q/Q 退出
        if(!strcmp(message, "q\n") || !strcmp(message, "Q\n"))
            break;

        // 将用户输入发送给服务器（不含额外终止符）
        write(sock, message, strlen(message));
        // 从服务器读取回显内容
        str_len = read(sock, message, BUF_SIZE);
        // 这里不手动添加 '\0'，因为服务器返回的是原字符串
        printf("Message from server: %s\n", message);
    }
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
