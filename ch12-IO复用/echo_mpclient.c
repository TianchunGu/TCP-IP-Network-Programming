#include <stdio.h>      // 标准输入输出：printf / fputs / fgets 等
#include <stdlib.h>     // 标准库：exit / atoi 等
#include <string.h>     // 字符串/内存操作：memset / strcmp / strlen 等
#include <unistd.h>     // POSIX：read / write / close / fork 等
#include <arpa/inet.h>  // 网络地址与字节序：inet_addr / htons 等
#include <sys/socket.h> // 套接字 API：socket / connect / shutdown 等

#define BUF_SIZE 30                 // 通信缓冲区大小：每次读/写最多 30 字节
void error_handling(char *message); // 错误处理函数：输出错误信息并退出
void read_routine(int sock, char *buf);  // 子进程读循环：不断从服务器读取并打印
void write_routine(int sock, char *buf); // 父进程写循环：不断从 stdin 读取并发送给服务器

int main(int argc, char* argv[])
{
    int serv_sock;                 // 客户端 TCP 套接字：连接服务器后用于收发数据
    struct sockaddr_in serv_addr;  // 服务器 IPv4 地址结构体
    pid_t pid;                     // fork 返回值：用于区分父/子进程
    char buf[BUF_SIZE];            // 通用缓冲区：fork 后父子进程各持有一份副本（地址空间独立）

    // -------------------- 参数检查 --------------------
    // 程序需要两个参数：服务器 IP 与端口
    // argv[1] = IP（例如 127.0.0.1）
    // argv[2] = port（例如 9190）
    if(argc != 3)
    {
        printf("Usage: %s <IP> <port>\n", argv[0]);
        exit(1); // 参数不正确则退出
    }

    // -------------------- 创建 TCP 套接字 --------------------
    // PF_INET     : IPv4
    // SOCK_STREAM : TCP（面向连接、字节流）
    // 0           : 让系统自动选择 TCP 协议（IPPROTO_TCP）
    serv_sock = socket(PF_INET, SOCK_STREAM, 0);

    // -------------------- 填充服务器地址 --------------------
    memset(&serv_addr, 0, sizeof(serv_addr));      // 清零结构体，避免未初始化字段
    serv_addr.sin_family = AF_INET;                // IPv4
    serv_addr.sin_addr.s_addr = inet_addr(argv[1]); // 点分十进制 IP -> 网络字节序 IPv4 地址
    serv_addr.sin_port = htons(atoi(argv[2]));     // 端口：atoi 转 int，htons 转网络字节序（大端）

    // -------------------- 连接服务器 --------------------
    // connect 触发 TCP 三次握手；成功后 serv_sock 进入“已连接”状态
    // 后续可直接使用 read/write 进行通信
    if(connect(serv_sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == -1)
        error_handling("connect() error");

    // -------------------- fork：将读写分离到两个进程并发执行 --------------------
    // 目的：同时支持
    // - 一边从键盘读取并发送（父进程）
    // - 一边接收服务器消息并打印（子进程）
    //
    // fork 后父子进程都持有同一个 TCP 连接对应的文件描述符 serv_sock
    // （它们引用同一个内核 socket 对象），因此可分别负责读/写。
    pid = fork();

    // fork 返回值语义：
    // - 子进程中 pid == 0
    // - 父进程中 pid > 0（为子进程 PID）
    // - 失败 pid < 0（本示例未处理失败情况）
    if(pid == 0)
        read_routine(serv_sock, buf);   // 子进程：只负责接收并显示服务器消息
    else
        write_routine(serv_sock, buf);  // 父进程：只负责读取用户输入并发送给服务器

    // 当各自的 routine 返回后，执行 close：
    // - 子进程可能在服务器关闭连接后返回（read 返回 0）
    // - 父进程可能在用户输入 q/Q 后 return（对写方向 shutdown）
    // 注意：父子进程都会 close 自己持有的描述符；
    // 真正释放连接资源要等所有引用都关闭后才发生。
    close(serv_sock);
    return 0;
}

void read_routine(int sock, char* buf)
{
    while (1)
    {
        // 从 socket 读取数据：
        // - 最多读 BUF_SIZE 字节
        // - 返回值 str_len：
        //   >0：读到的数据字节数
        //   =0：对端关闭连接（EOF）
        //   <0：出错（本示例未处理 -1）
        int str_len = read(sock, buf, BUF_SIZE);

        // 若返回 0，表示服务器关闭连接或写端关闭且数据已读尽
        if (str_len == 0)
            return;

        // 将读取的数据作为 C 字符串打印时，需要手动加 '\0'
        // 注意：若 str_len == BUF_SIZE，则 buf[str_len] 会越界；
        // 更稳妥的方式通常是 read(sock, buf, BUF_SIZE-1) 预留结束符空间。
        buf[str_len] = 0;

        // 输出服务器消息
        printf("Message from server: %s\n", buf);
    }
}

void write_routine(int sock, char* buf)
{
    while (1)
    {
        // 从标准输入读取一行：
        // - 最多读取 BUF_SIZE-1 个字符
        // - 自动添加 '\0'
        // - 通常会保留换行符 '\n'（缓冲区足够时）
        fgets(buf, BUF_SIZE, stdin);

        // 若输入 q/Q 则结束发送：
        // 因为 fgets 会保留 '\n'，所以比较的是 "q\n" / "Q\n"
        if(!strcmp(buf, "q\n") || !strcmp(buf, "Q\n"))
        {
            // shutdown(sock, SHUT_WR)：关闭“写方向”（半关闭）
            // - 向对端发送 FIN，表示本端不再发送数据
            // - 读方向仍然可用（子进程仍能接收服务器可能发送的剩余数据）
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
