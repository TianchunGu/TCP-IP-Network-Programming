#include <stdio.h>      // 标准输入输出：printf / fputs / fgets / fputc 等
#include <stdlib.h>     // 标准库：exit / atoi 等
#include <string.h>     // 字符串与内存操作：memset / strcmp / strlen 等
#include <unistd.h>     // POSIX I/O：read / write / close 等（这里用来替代 sendto/recvfrom）
#include <arpa/inet.h>  // 网络地址转换：inet_addr / htons 等
#include <sys/socket.h> // socket / connect 以及 sockaddr 相关定义

#define BUF_SIZE 30     // 应用层缓冲区大小：一次最多读写 29 字节 + 1 个 '\0'
void error_handling(char *message); // 错误处理函数声明：打印错误并退出

int main(int argc, char* argv[])
{
    int sock;                           // UDP 套接字文件描述符
    char message[BUF_SIZE];             // 发送/接收数据缓冲区
    int str_len;                        // 实际读到/收到的数据长度（字节数）
    struct sockaddr_in serv_addr, clnt_addr; // serv_addr：服务器地址；clnt_addr：客户端地址（本代码中注释掉的 recvfrom 会用到）
    socklen_t addr_sz;                  // 地址结构长度变量（配合 recvfrom 使用；本代码中也被注释掉）

    // 程序参数检查：期望从命令行传入服务器 IP 与端口
    // argv[1] = IP（如 127.0.0.1），argv[2] = port（如 9190）
    if(argc != 3)
    {
        printf("Usage: %s <IP> <port>\n", argv[0]);
        exit(1); // 参数不对直接退出
    }

    // 创建 UDP 套接字：
    // PF_INET  : IPv4 协议族
    // SOCK_DGRAM: 数据报套接字（UDP）
    // 0        : 让系统自动选择 UDP 协议（IPPROTO_UDP）
    sock = socket(PF_INET, SOCK_DGRAM, 0);
    if(sock == -1)
        error_handling("socket() error"); // 创建失败则退出

    // 将服务器地址结构清零，避免未初始化字段导致不可预期行为
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;                 // IPv4
    serv_addr.sin_addr.s_addr = inet_addr(argv[1]); // 将点分十进制 IP 字符串转成网络字节序的 32 位地址
    serv_addr.sin_port = htons(atoi(argv[2]));      // 端口号：atoi 转整数，再用 htons 转网络字节序（大端）

    // 对 UDP 套接字调用 connect：
    // UDP 本身是无连接协议，但 connect 会给该 UDP 套接字“绑定默认对端地址”
    // 之后：
    // 1) 可以直接用 write/send 发送，不必每次提供目的地址
    // 2) 可以直接用 read/recv 接收，并且只接收来自该对端的数据报（其他来源会被内核丢弃）
    // 注意：UDP 的 connect 不会建立类似 TCP 的握手连接，只是设置默认对端与过滤规则
    connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr));

    while (1)
    {
        // 提示用户输入；stdout 用 fputs 更直观
        fputs("Insert message(q/Q to quit): ", stdout);

        // 从标准输入读取一行，最多读取 BUF_SIZE-1 个字符，确保有 '\0' 结尾
        // 可能包含换行符 '\n'
        fgets(message, sizeof(message), stdin);

        // 若用户输入 "q\n" 或 "Q\n" 则退出循环
        // 注意：这里比较的是包含换行符的字符串，因为 fgets 会把 '\n' 一起读进来（如果空间足够）
        if(!strcmp(message, "q\n") || !strcmp("Q\n", message))
            break;

        // 下面两段注释展示了“未 connect 的 UDP 典型写法”：
        // sendto/recvfrom 每次都需要指定地址或获取发送方地址。
        //
        // sendto(sock, message, strlen(message), 0,
        //     (struct sockaddr*)&serv_addr, sizeof(serv_addr));
        //
        // 但由于前面已经 connect(sock, serv_addr)，这里改用 write：
        // write 会把数据报发给“默认对端 serv_addr”
        // strlen(message) 包含用户输入内容（通常含 '\n'），不包含结尾的 '\0'
        write(sock, message, strlen(message));

        // 同理，未 connect 的 UDP 接收通常用 recvfrom，并得到发送方 clnt_addr：
        //
        // addr_sz = sizeof(clnt_addr);
        // str_len = recvfrom(sock, message, sizeof(message) - 1, 0,
        //     (struct sockaddr*)&clnt_addr, &addr_sz);
        //
        // connect 后可直接 read，只接收来自默认对端的数据报：
        // sizeof(message) - 1：预留一个字节给字符串结束符 '\0'
        str_len = read(sock, message, sizeof(message) - 1);

        // 将收到的数据当作 C 字符串使用时，需要手动补 '\0'
        // read 返回的是字节数，不会自动添加字符串结束符
        message[str_len] = 0;

        // 打印服务器回显/响应内容
        printf("Message from server: %s\n", message);
    }

    // 关闭套接字，释放系统资源
    close(sock);

    return 0;
}

void error_handling(char *message)
{
    // 将错误信息输出到标准错误流 stderr，便于与正常输出区分
	fputs(message, stderr);
	fputc('\n', stderr);
	exit(1); // 非正常退出
}
