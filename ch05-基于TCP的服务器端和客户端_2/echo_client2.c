#include <stdio.h>      // printf, fputs, fgets, stdout
#include <stdlib.h>     // exit, atoi
#include <string.h>     // memset, strcmp, strlen
#include <unistd.h>     // read, write, close
#include <arpa/inet.h>  // inet_addr, htons, sockaddr_in
#include <sys/socket.h> // socket, connect

#define BUF_SIZE 1024 // 读写缓冲区大小
void error_handling(char *message);

int main(int argc, char* argv[])
{
    int sock;                     // 客户端套接字描述符
    struct sockaddr_in serv_addr; // 服务器地址
    int str_len, recv_tot, recv_cur; // 发送长度、累计接收长度、单次接收长度
    char message[BUF_SIZE];       // 发送/接收缓冲区

    // 创建 TCP 套接字
    if( (sock = socket(PF_INET, SOCK_STREAM, 0)) == -1)
        error_handling("socket() error");
    
    // 这里假定已正确传入 IP 和端口参数
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    // IP 字符串转换为网络字节序
    serv_addr.sin_addr.s_addr = inet_addr(argv[1]);
    // 端口号转换为网络字节序
    serv_addr.sin_port = htons(atoi(argv[2]));

    // 连接服务器
    if(connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == -1)
        error_handling("connect() error");
    
    // 交互式回显：输入一行 -> 发送 -> 循环接收直到长度一致
    while(1)
    {
        fputs("Input Message(Q/q to quit): ", stdout);
        fgets(message, BUF_SIZE, stdin);

        // 输入 q/Q 退出
        if(!strcmp(message, "q\n") || !strcmp(message, "Q\n"))
            break;
        
        // 发送当前输入
        str_len = write(sock, message, strlen(message));
        // 处理 TCP 粘包/拆包：确保接收长度与发送长度一致
        recv_tot = 0;
        while (recv_tot < str_len)
        {
            recv_cur = read(sock, message, BUF_SIZE - 1);
            if(recv_cur == -1)
                error_handling("write() error");
            recv_tot += recv_cur;
        }
        // 手动添加字符串结束符，便于打印
        message[str_len] = 0;
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
