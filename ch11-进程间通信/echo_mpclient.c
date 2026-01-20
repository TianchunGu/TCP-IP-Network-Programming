#include <stdio.h>      // 标准输入输出：printf / fputs / fgets 等
#include <stdlib.h>     // 标准库：exit / atoi 等
#include <string.h>     // 字符串/内存操作：memset / strcmp / strlen 等
#include <unistd.h>     // POSIX：read / write / close / fork 等
#include <arpa/inet.h>  // 网络地址与字节序：inet_addr / htons 等
#include <sys/socket.h> // 套接字 API：socket / connect / shutdown 等

#define BUF_SIZE 30                 // 通信缓冲区大小：读/写都使用该固定大小
void error_handling(char *message); // 错误处理：输出错误信息并退出
void read_routine(int sock, char *buf);  // 读线程（子进程）逻辑：持续从服务器读取并打印
void write_routine(int sock, char *buf); // 写线程（父进程）逻辑：持续从键盘读取并发送给服务器

int main(int argc, char* argv[])
{
    int serv_sock;                  // 客户端 TCP 套接字（连接到服务器后用于通信）
    struct sockaddr_in serv_addr;   // 服务器 IPv4 地址结构体
    pid_t pid;                      // fork() 返回值：用于区分父/子进程
    char buf[BUF_SIZE];             // 通信缓冲区：fork 后父子进程各自拥有一份副本

    // -------------------- 参数检查 --------------------
    // 需要两个参数：服务器 IP 与端口
    // argv[1] = IP（如 127.0.0.1）
    // argv[2] = port（如 9190）
    if(argc != 3)
    {
        printf("Usage: %s <IP> <port>\n", argv[0]);
        exit(1); // 参数不对直接退出
    }

    // -------------------- 创建 TCP 套接字 --------------------
    // PF_INET     : IPv4
    // SOCK_STREAM : TCP（面向连接、字节流）
    // 0           : 自动选择 TCP 协议（IPPROTO_TCP）
    serv_sock = socket(PF_INET, SOCK_STREAM, 0);

    // -------------------- 填充服务器地址 --------------------
    memset(&serv_addr, 0, sizeof(serv_addr));      // 清零，避免未初始化字段
    serv_addr.sin_family = AF_INET;                // IPv4
    serv_addr.sin_addr.s_addr = inet_addr(argv[1]); // 点分十进制字符串 IP -> 网络字节序 IPv4 地址
    serv_addr.sin_port = htons(atoi(argv[2]));     // 端口：atoi 转 int，再 htons 转网络字节序

    // -------------------- 连接服务器 --------------------
    // connect 触发 TCP 三次握手；成功后 serv_sock 进入“已连接”状态
    if(connect(serv_sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == -1)
        error_handling("connect() error");

    // -------------------- fork：读写分离并发 --------------------
    // 目的：让“接收服务器消息”和“用户输入发送”同时进行。
    // fork 后：
    // - 子进程负责 read_routine：阻塞读 socket，收到就打印
    // - 父进程负责 write_routine：阻塞读 stdin，输入就发给服务器
    //
    // 注意：fork 后父子进程都会持有 serv_sock（引用同一个内核 socket 对象），
    // 因此它们对该 socket 的读写作用于同一条 TCP 连接。
    pid = fork();

    // fork 返回值语义：
    // - 子进程中 pid == 0
    // - 父进程中 pid > 0（为子进程 PID）
    // - 失败 pid < 0（此示例未显式处理失败情况）
    if(pid == 0)
        read_routine(serv_sock, buf);   // 子进程：只负责接收并打印
    else
        write_routine(serv_sock, buf);  // 父进程：只负责发送用户输入

    // 父子进程各自从各自的 routine 返回后都会执行 close
    // 真正的 TCP 连接资源会在“所有引用都关闭”后才由内核释放
    close(serv_sock);
    return 0;
}

void read_routine(int sock, char* buf)
{
    while (1)
    {
        // 从 socket 读取数据：
        // - 这里最多读取 BUF_SIZE 字节
        // - read 返回实际读取字节数：
        //    >0：读到数据
        //     0：对端关闭连接（EOF）
        //    -1：出错（本示例未处理 -1）
        int str_len = read(sock, buf, BUF_SIZE);

        // 如果 read 返回 0，说明服务器已关闭连接或写端关闭且数据已读尽
        if (str_len == 0)
            return;

        // 将读取到的数据当作 C 字符串输出时，手动补 '\0'
        // 注意：若 str_len == BUF_SIZE，则 buf[str_len] 会越界；
        // 本示例通常假设单次接收不会恰好等于 BUF_SIZE（教学示例的常见简化）
        buf[str_len] = 0;

        // 输出服务器发来的消息
        printf("Message from server: %s\n", buf);
    }
}

void write_routine(int sock, char* buf)
{
    while (1)
    {
        // 从标准输入读取一行到 buf：
        // - 最多读取 BUF_SIZE-1 个字符，并自动补 '\0'
        // - 通常会把换行符 '\n' 也读进来（缓冲区足够时）
        fgets(buf, BUF_SIZE, stdin);

        // 若输入 q 或 Q 则结束发送：
        // 这里比较的是包含 '\n' 的字符串（因为 fgets 会保留 '\n'）
        if(!strcmp(buf, "q\n") || !strcmp(buf, "Q\n"))
        {
            // shutdown(sock, SHUT_WR)：关闭该 TCP 连接的“写方向”（半关闭）
            // - 内核会向对端发送 FIN，表示“我不再发送数据”
            // - 但读方向仍可继续接收对端发送的数据
            // 这样即使用户不再输入，子进程仍可以把服务器最后发送的数据读完并打印
            shutdown(sock, SHUT_WR);
            return;
        }

        // 将用户输入发送给服务器
        // strlen(buf) 不包含 '\0'，一般包含 '\n'
        // 注意：TCP 是字节流协议，write 不保证一次写完全部数据（示例简化处理）
        write(sock, buf, strlen(buf));
    }
}

void error_handling(char *message)
{
    // 输出错误信息到标准错误流 stderr
    fputs(message, stderr);
    fputc('\n', stderr);
    exit(1); // 非正常退出
}
