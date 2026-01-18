#include <stdio.h>      // 标准输入输出：printf / fputs / fgets / fputc 等
#include <stdlib.h>     // 标准库：exit / atoi 等
#include <string.h>     // 字符串处理：memset / strcmp / strlen 等
#include <unistd.h>     // POSIX：close 等（本程序主要用于 close）
#include <arpa/inet.h>  // 网络相关：inet_addr / htons 等
#include <sys/socket.h> // 套接字 API：socket / sendto / recvfrom 等

#define BUF_SIZE 30     // 缓冲区大小：用于存放发送/接收的消息
void error_handling(char *message); // 错误处理函数声明：打印错误信息并退出

int main(int argc, char *argv[])
{
    int sock;                       // UDP 套接字文件描述符
    char message[BUF_SIZE];         // 发送/接收缓冲区
    int str_len;                    // 实际接收到的数据长度（字节数）

    struct sockaddr_in serv_addr, clnt_addr; // serv_addr：服务器地址；clnt_addr：接收数据时记录发送方地址
    socklen_t clnt_addr_sz;                 // clnt_addr 结构体的大小（recvfrom 需要传入/返回该长度）

    // 参数检查：程序需要两个参数：服务器 IP 与端口
    // argv[1] = IP（如 127.0.0.1），argv[2] = port（如 9190）
    if(argc != 3)
    {
        printf("Usage: %s <IP> <port>\n", argv[0]);
        exit(1); // 参数不正确，直接退出
    }

    // 创建 UDP 套接字：
    // PF_INET   : IPv4 协议族
    // SOCK_DGRAM: UDP 数据报套接字
    // 0         : 让系统自动选择 UDP 协议（IPPROTO_UDP）
    sock = socket(PF_INET, SOCK_DGRAM, 0);
    if(sock == -1)
        error_handling("socket() error"); // 创建失败则打印错误并退出

    // 初始化服务器地址结构体为 0，避免未初始化字段带来不可预期结果
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;                 // IPv4
    serv_addr.sin_addr.s_addr = inet_addr(argv[1]); // 将点分十进制 IP 字符串转换为网络字节序的 32 位地址
    serv_addr.sin_port = htons(atoi(argv[2]));      // 端口：atoi 转 int，再 htons 转网络字节序（大端）

    // 进入循环：不断从用户读取消息，发送给服务器并等待服务器回包
    while (1)
    {
        // 提示用户输入
        fputs("Insert message: (q/Q to quit): ", stdout);

        // 从标准输入读取一行到 message
        // 最多读取 BUF_SIZE-1 个字符，并自动追加 '\0'
        // 通常会包含用户输入的换行符 '\n'（若缓冲区足够）
        fgets(message, BUF_SIZE, stdin);

        // 若输入为 "q\n" 或 "Q\n" 则退出循环
        // 注意：因为 fgets 会把 '\n' 读进来，所以比较时也包含 '\n'
        if(!strcmp(message, "q\n") || !strcmp(message, "Q\n"))
            break;

        // sendto：向指定地址发送 UDP 数据报
        // sock                         : UDP 套接字
        // message, strlen(message)      : 发送的内容与长度（不包含 '\0'）
        // 0                            : flags（通常为 0）
        // (struct sockaddr*)&serv_addr : 目的地址（服务器地址）
        // sizeof(serv_addr)            : 目的地址结构体长度
        sendto(sock, message, strlen(message), 0,
            (struct sockaddr*)&serv_addr, sizeof(serv_addr));

        // recvfrom：接收 UDP 数据报
        // clnt_addr_sz 必须先设置为 clnt_addr 的大小，内核会写回实际长度
        // clnt_addr 用来保存“发送方地址”（这里一般就是服务器地址）
        clnt_addr_sz = sizeof(clnt_addr);
        str_len = recvfrom(sock, message, BUF_SIZE, 0,
            (struct sockaddr*)&clnt_addr, &clnt_addr_sz);

        // recvfrom 返回接收到的字节数 str_len
        // 由于接收的数据是“原始字节”，不会自动加字符串结束符，需要手动补 '\0'
        message[str_len] = 0;

        // 打印服务器返回的消息（通常是回显/响应）
        printf("Message from server: %s\n", message);
    }

    // 关闭套接字，释放资源
    close(sock);

    return 0;
}

void error_handling(char *message)
{
    // 将错误信息输出到标准错误流 stderr
	fputs(message, stderr);
	fputc('\n', stderr);
	exit(1); // 非正常退出
}
