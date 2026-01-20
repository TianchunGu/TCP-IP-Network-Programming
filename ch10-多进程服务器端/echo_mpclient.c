#include <stdio.h>      // 标准输入输出：printf / fprintf / fgets 等
#include <stdlib.h>     // 标准库：exit / atoi 等
#include <string.h>     // 字符串/内存操作：memset / strcmp / strlen 等
#include <unistd.h>     // POSIX：read / write / close / fork 等
#include <arpa/inet.h>  // 网络地址转换：inet_addr / htons 等
#include <sys/socket.h> // 套接字 API：socket / connect / shutdown 等

#define BUF_SIZE 30                 // 缓冲区大小：读写都使用该固定长度
void error_handling(char *message); // 错误处理：打印错误并退出
void read_routine(int sock, char *buf);  // 子进程读循环：不断从服务器读并打印
void write_routine(int sock, char *buf); // 父进程写循环：不断从 stdin 读并发给服务器

int main(int argc, char* argv[])
{
    int serv_sock;                   // TCP 套接字（客户端），连接到服务器后用于读写
    struct sockaddr_in serv_addr;    // 服务器地址结构体（IPv4）
    pid_t pid;                       // fork() 返回值：用于区分父/子进程
    char buf[BUF_SIZE];              // 通用缓冲区：父子进程各自使用（fork 后各有一份拷贝）

    // -------------------- 参数检查 --------------------
    // 需要两个参数：服务器 IP 与端口
    // argv[1] = IP（例如 127.0.0.1）
    // argv[2] = port（例如 9190）
    if(argc != 3)
    {
        printf("Usage: %s <IP> <port>\n", argv[0]);
        exit(1);
    }

    // -------------------- 创建 TCP 套接字 --------------------
    // PF_INET     : IPv4
    // SOCK_STREAM : TCP（面向连接的字节流）
    // 0           : 让系统自动选择 TCP 协议（IPPROTO_TCP）
    serv_sock = socket(PF_INET, SOCK_STREAM, 0);

    // -------------------- 填充服务器地址 --------------------
    memset(&serv_addr, 0, sizeof(serv_addr));     // 清零，避免未初始化字段
    serv_addr.sin_family = AF_INET;               // IPv4
    serv_addr.sin_addr.s_addr = inet_addr(argv[1]); // 将点分十进制 IP 字符串转为网络字节序 IPv4 地址
    serv_addr.sin_port = htons(atoi(argv[2]));    // 端口：atoi 转 int，htons 转网络字节序（大端）

    // -------------------- 发起连接 --------------------
    // connect 会触发 TCP 三次握手；成功后 serv_sock 变为“已连接套接字”
    if(connect(serv_sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == -1)
        error_handling("connect() error");

    // -------------------- fork：分离读写 --------------------
    // 为了让“接收服务器消息”和“从键盘输入发送”同时进行：
    // - 子进程负责 read_routine：持续读取服务器消息并打印
    // - 父进程负责 write_routine：持续读取 stdin 并发送给服务器
    //
    // fork 后父子进程都会拥有 serv_sock 的文件描述符（引用同一个内核 socket 对象）
    // 因此父子进程的读写会作用于同一个 TCP 连接
    pid = fork();

    // fork 的返回值语义：
    // - 子进程中 pid == 0
    // - 父进程中 pid > 0（为子进程 PID）
    // - 失败时 pid < 0（本示例未显式处理失败情况）
    if(pid == 0)
        read_routine(serv_sock, buf);   // 子进程：只读（阻塞等待服务器数据）
    else
        write_routine(serv_sock, buf);  // 父进程：只写（等待用户输入并发送）

    // 当 read_routine 或 write_routine 返回后，执行 close：
    // 注意：父子进程都会执行到这里，各自关闭自己持有的描述符引用
    // 真正的连接资源会在所有引用都 close 之后才被内核释放
    close(serv_sock);
    return 0;
}

void read_routine(int sock, char* buf)
{
    while (1)
    {
        // 从 TCP 连接读取数据：
        // BUF_SIZE：最多读 BUF_SIZE 字节
        // 返回值 str_len：
        // - >0：读到的字节数
        // - 0 ：对端关闭连接（EOF），说明服务器不再发送数据
        // - -1：出错（本示例未处理负值）
        int str_len = read(sock, buf, BUF_SIZE);

        // 若读到 0，表示服务器关闭了连接（或写端已关闭且数据读尽）
        if (str_len == 0)
            return;

        // 将读到的数据当作 C 字符串输出时，需要手动添加 '\0'
        // 注意：如果 str_len == BUF_SIZE，则 buf[str_len] 会越界；
        // 本示例假定服务器发送长度不会刚好等于 BUF_SIZE
        buf[str_len] = 0;

        // 打印服务器消息
        printf("Message from server: %s\n", buf);
    }
}

void write_routine(int sock, char* buf)
{
    while (1)
    {
        // 从标准输入读取一行
        // 最多读取 BUF_SIZE-1 个字符，并自动补 '\0'
        // 通常会把 '\n' 一并读进来（缓冲区足够时）
        fgets(buf, BUF_SIZE, stdin);

        // 输入 q 或 Q 则退出发送循环
        if(!strcmp(buf, "q\n") || !strcmp(buf, "Q\n"))
        {
            // shutdown(sock, SHUT_WR)：关闭“写方向”
            // - 这会向对端发送 FIN，表示“我不再发送数据了”
            // - 但读方向仍然保留：对端仍可继续向本端发送数据，
            //   子进程 read_routine 可以继续把剩余数据读完并打印
            //
            // 这比直接 close 更适合“半关闭”场景：发送结束但仍想接收对方消息
            shutdown(sock, SHUT_WR);
            return;
        }

        // 将输入内容发送给服务器
        // strlen(buf) 不包含 '\0'，一般包含 '\n'
        // 注意：TCP 是字节流，write 不保证一次写完全部数据（示例简化处理）
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
