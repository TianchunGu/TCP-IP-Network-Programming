#include <stdio.h>      // printf, fputs, stdout
#include <stdlib.h>     // exit, atoi
#include <string.h>     // memset
#include <unistd.h>     // read, write, close
#include <arpa/inet.h>  // htonl, htons, sockaddr_in
#include <sys/socket.h> // socket, bind, listen, accept

#define BUF_SIZE 1024 // 读写缓冲区大小
void error_handling(char *message);

int main(int argc, char *argv[])
{
    int serv_sock, clnt_sock; // 监听套接字与客户端连接套接字
    char message[BUF_SIZE];   // 收发缓冲区
    int str_len, i;           // 读取字节数与循环计数

    struct sockaddr_in serv_adr; // 服务器地址
    struct sockaddr_in clnt_adr; // 客户端地址
    socklen_t clnt_adr_sz;       // 客户端地址结构体大小

    // 参数校验：需要端口号
    if (argc != 2)
    {
        printf("Usage : %s <port>\n", argv[0]);
        exit(1);
    }

    // 创建 TCP 监听套接字
    serv_sock = socket(PF_INET, SOCK_STREAM, 0);
    if (serv_sock == -1)
        error_handling("socket() error");

    // 初始化并设置服务器地址
    memset(&serv_adr, 0, sizeof(serv_adr));
    serv_adr.sin_family = AF_INET;
    // 绑定到本机任意地址
    serv_adr.sin_addr.s_addr = htonl(INADDR_ANY);
    // 端口号转换为网络字节序
    serv_adr.sin_port = htons(atoi(argv[1]));

    // 绑定地址与端口
    if (bind(serv_sock, (struct sockaddr *)&serv_adr, sizeof(serv_adr)) == -1)
        error_handling("bind() error");

    // 开始监听
    if (listen(serv_sock, 5) == -1)
        error_handling("listen() error");

    clnt_adr_sz = sizeof(clnt_adr);

    // 依次处理最多 5 个客户端连接
    for (i = 0; i < 5; i++)
    {
        // 阻塞等待客户端连接
        clnt_sock = accept(serv_sock, (struct sockaddr *)&clnt_adr, &clnt_adr_sz);
        if (clnt_sock == -1)
            error_handling("accept() error");
        else
            printf("Connected client %d \n", i + 1);

        // 回显：读取多少就原样写回
        while ((str_len = read(clnt_sock, message, BUF_SIZE)) != 0)
            write(clnt_sock, message, str_len);

        // 当前客户端处理结束，关闭连接
        close(clnt_sock);
    }

    // 关闭监听套接字
    close(serv_sock);
    return 0;
}

void error_handling(char *message)
{
    // 将错误信息输出到标准错误并终止程序
    fputs(message, stderr);
    fputc('\n', stderr);
    exit(1);
}
