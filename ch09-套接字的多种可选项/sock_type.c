#include <stdio.h>      // 标准输入输出：printf / fputs / fputc 等
#include <stdlib.h>     // 标准库：exit 等
#include <unistd.h>     // POSIX（本程序中未直接使用，但常与 socket 示例一并包含）
#include <sys/socket.h> // 套接字 API：socket / getsockopt，以及 SOL_SOCKET / SO_TYPE / SOCK_STREAM / SOCK_DGRAM 等

void error_handling(char *message); // 错误处理函数声明：打印错误信息并退出

int main(int argc, char* argv[])
{
    int tcp_sock, udp_sock; // tcp_sock：TCP 套接字（SOCK_STREAM）；udp_sock：UDP 套接字（SOCK_DGRAM）
    int sock_type;          // 用于保存 getsockopt 读取到的套接字类型值（即 SO_TYPE 的返回值）
    socklen_t optlen;       // getsockopt 的 optlen 参数：传入/传出选项值长度（字节数）
    int state;              // 系统调用返回值（0 成功，-1 失败；此处用 if(state) 判断非 0 为错误）

    // optlen 表示我们提供的 sock_type 变量大小
    // getsockopt 会往 sock_type 写入 SO_TYPE 的值，同时可能更新 optlen
    optlen = sizeof(sock_type);

    // 创建两个套接字用于对比：
    // PF_INET     : IPv4
    // SOCK_STREAM : TCP（面向连接的字节流）
    // SOCK_DGRAM  : UDP（无连接的数据报）
    // 0           : 让系统自动选择协议（对 SOCK_STREAM 为 TCP，对 SOCK_DGRAM 为 UDP）
    tcp_sock = socket(PF_INET, SOCK_STREAM, 0);
    udp_sock = socket(PF_INET, SOCK_DGRAM, 0);

    // 打印宏常量的数值（不同系统实现可能不同，但语义相同）
    printf("SOCK_STREAM: %d\n", SOCK_STREAM);
    printf("SOCK_DGRAM: %d\n", SOCK_DGRAM);

    // -------------------- 查询 tcp_sock 的套接字类型（SO_TYPE） --------------------
    // getsockopt 参数说明：
    // tcp_sock            : 目标套接字
    // SOL_SOCKET          : socket 层级通用选项
    // SO_TYPE             : 查询“套接字类型”（返回 SOCK_STREAM / SOCK_DGRAM 等）
    // (void*)&sock_type   : 输出缓冲区地址，用于接收返回值
    // &optlen             : 输入/输出参数：输入为缓冲区大小，输出为实际写入大小
    state = getsockopt(tcp_sock, SOL_SOCKET, SO_TYPE, (void*)&sock_type, &optlen);
    if(state)
        error_handling("getsockopt() error");
    printf("Socket type one: %d\n", sock_type); // 期望输出与 SOCK_STREAM 相同的值

    // -------------------- 查询 udp_sock 的套接字类型（SO_TYPE） --------------------
    // 注意：这里复用 sock_type 与 optlen
    // 一般情况下 optlen 仍是 sizeof(int)，但严谨做法是每次调用前都重新设 optlen
    state = getsockopt(udp_sock, SOL_SOCKET, SO_TYPE, (void*)&sock_type, &optlen);
    if(state)
        error_handling("getsockopt() error");
    printf("Socket type two: %d\n", sock_type); // 期望输出与 SOCK_DGRAM 相同的值

    return 0;
}

void error_handling(char *message)
{
    // 将错误信息输出到标准错误流 stderr
    fputs(message, stderr);
    fputc('\n', stderr);
    exit(1); // 非正常退出
}
