#include <stdio.h>      // printf, fputs, stderr
#include <stdlib.h>     // exit, atoi
#include <string.h>     // memset
#include <unistd.h>     // write, close
#include <arpa/inet.h>  // htonl, htons, sockaddr_in
#include <sys/types.h>  // 基本系统数据类型
#include <sys/socket.h> // socket, bind, listen, accept

// 统一错误处理：输出错误信息并终止程序
void error_handling(char *message);

int main(int argc, char *argv[])
{
    int serv_sock; // 服务器监听套接字
    int clnt_sock; // 客户端连接套接字
    
    struct sockaddr_in serv_addr; // 服务器地址
    struct sockaddr_in clnt_addr; // 客户端地址
    socklen_t clnt_addr_sz;       // 客户端地址结构体大小

    char message[] = "Hello World\n"; // 发送给客户端的固定消息
    // 参数校验：需要端口号
    if(argc != 2)
    {
        printf("Usage: %s <port>", argv[0]);
        exit(1);
    }

    // 创建 TCP 监听套接字
    serv_sock = socket(PF_INET, SOCK_STREAM, 0);
    if(serv_sock == -1)
        error_handling("socket() error!");
    // 初始化服务器地址结构体
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    // 绑定到任意本地地址
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    // 端口号转换为网络字节序
    serv_addr.sin_port = htons(atoi(argv[1]));

    // 将套接字绑定到指定 IP/端口
    if(bind(serv_sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == -1)
        error_handling("bind() error!");
    
    // 进入监听状态，等待客户端连接
    if(listen(serv_sock, 5) == -1)
        error_handling("listen() error");

    // 接受一个客户端连接（阻塞直到有连接到达）
    clnt_addr_sz = sizeof(clnt_addr);
    clnt_sock = accept(serv_sock, (struct sockaddr*)&clnt_addr, &clnt_addr_sz);
    if(clnt_sock == -1)
        error_handling("accept() error!");
    
    // 向客户端写入消息
    write(clnt_sock, message, sizeof(message));
    // 关闭套接字，释放资源
    close(serv_sock);
    close(clnt_sock);

    return 0;
}

void error_handling(char *message)
{
    // 将错误信息输出到标准错误并终止程序
    fputs(message, stderr);
    fputc('\n', stderr);
    exit(1);
}
