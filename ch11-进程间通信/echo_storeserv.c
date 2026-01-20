#include <stdio.h>      // 标准输入输出：printf / puts / FILE / fopen / fwrite / fclose 等
#include <stdlib.h>     // 标准库：exit / atoi 等
#include <string.h>     // 内存操作：memset 等
#include <unistd.h>     // POSIX：read / write / close / fork / pipe 等
#include <signal.h>     // 信号处理：sigaction / sigemptyset / SIGCHLD 等
#include <sys/wait.h>   // 子进程回收：waitpid / WNOHANG 等
#include <arpa/inet.h>  // 网络字节序转换：htonl / htons 等
#include <sys/socket.h> // 套接字 API：socket / bind / listen / accept 等

#define BUF_SIZE 100                 // 通信缓冲区大小：服务器与客户端读写、以及管道读写都用该大小
void error_handling(char *message);  // 错误处理：打印错误信息并退出
void read_childproc(int sig);        // SIGCHLD 处理函数：回收退出的子进程，避免僵尸进程

int main(int argc, char* argv[])
{
    int serv_sock, clnt_sock;                 // serv_sock：监听套接字；clnt_sock：与某个客户端连接后的套接字
    struct sockaddr_in serv_addr, clnt_addr;  // serv_addr：服务器地址；clnt_addr：客户端地址（accept 填充）
    socklen_t addr_sz;                        // accept 用的地址长度
    int fds[2];                               // 管道的两个端：fds[0] 读端，fds[1] 写端

    pid_t pid;                 // fork 返回值：区分父/子进程
    int str_len, state;        // str_len：read 的返回长度；state：系统调用状态值
    struct sigaction act;      // 设置信号处理行为
    char buf[BUF_SIZE];        // 网络收发缓冲区（回显 + 写入管道）

    // -------------------- 参数检查 --------------------
    // 服务器程序只需要一个参数：监听端口
    if(argc != 2)
    {
        printf("Usage: %s <port>\n", argv[0]);
        exit(1);
    }

    // -------------------- 注册 SIGCHLD 处理器 --------------------
    // 子进程退出时会向父进程发送 SIGCHLD，父进程在处理器里 waitpid 回收子进程，避免僵尸进程。
    act.sa_handler = read_childproc;   // 指定 SIGCHLD 到来时调用 read_childproc
    sigemptyset(&act.sa_mask);         // 处理信号时不额外屏蔽其他信号
    act.sa_flags = 0;                  // 不设置额外标志（例如 SA_RESTART）
    state = sigaction(SIGCHLD, &act, 0); // 安装 SIGCHLD 信号处理器
    // 注意：此处未判断 state 是否为 -1；真实工程应检查是否安装成功

    // -------------------- 创建监听 socket 并绑定端口 --------------------
    serv_sock = socket(PF_INET, SOCK_STREAM, 0); // TCP 监听套接字
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;                // IPv4
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY); // 绑定所有网卡地址（0.0.0.0）
    serv_addr.sin_port = htons(atoi(argv[1]));     // 监听端口

    if(bind(serv_sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == -1)
        error_handling("bind() error");
    if(listen(serv_sock, 5) == -1)
        error_handling("listen() error");

    // -------------------- 创建匿名管道 --------------------
    // pipe(fds) 创建一个进程间通信通道（单向）：
    // - fds[0]：读端
    // - fds[1]：写端
    // fork 后父子进程都会继承这两个文件描述符，从而可以实现父子进程之间的数据传输
    pipe(fds);

    // -------------------- fork：创建“日志写入进程” --------------------
    // 子进程专门从管道读数据并写入文件 echomsg.txt，相当于一个简单的日志收集器
    pid = fork();
    if(pid == 0)
    {
        // -------------------- 日志子进程（只负责从管道读取并写文件） --------------------
        FILE* fp = fopen("echomsg.txt", "wb"); // 以二进制写方式打开文件（若存在则截断）
        char msgbuf[BUF_SIZE];                 // 管道读取缓冲区

        // 这里固定循环 10 次读取并写入文件：
        // 每次 read 从 fds[0]（管道读端）读取最多 BUF_SIZE 字节
        // 返回值 len 为实际读取到的字节数
        for(int i = 0; i < 10; i++)
        {
            int len = read(fds[0], msgbuf, BUF_SIZE);   // 从管道读
            fwrite((void*)msgbuf, 1, len, fp);          // 写到文件：每次写 len 字节
        }

        fclose(fp); // 关闭文件
        return 0;   // 日志子进程结束（父进程会收到 SIGCHLD 并回收）
    }

    // -------------------- 主循环：不断 accept 客户端，并 fork 处理 --------------------
    while(1)
    {
        // accept：等待新客户端连接，成功返回与该客户端通信的 clnt_sock
        addr_sz = sizeof(clnt_addr);
        clnt_sock = accept(serv_sock, (struct sockaddr*)&clnt_addr, &addr_sz);

        // accept 失败（返回 -1）时，直接继续下一轮
        // 失败原因可能包括：被信号中断（例如 SIGCHLD）等
        if(clnt_sock == -1)
            continue;
        else
            puts("New clinet connected...");

        // 为每个客户端连接创建一个子进程处理（典型的“多进程并发服务器”模型）
        pid = fork();
        if(pid == 0)
        {
            // -------------------- 客户端处理子进程 --------------------
            // 子进程不需要监听套接字（serv_sock），关闭它减少资源占用
            close(serv_sock);

            // 回显服务：读取客户端发来的数据并原样写回
            // 同时将该数据写入管道 fds[1]，交给日志进程写入文件
            while((str_len = read(clnt_sock, buf, BUF_SIZE)) != 0)
            {
                write(clnt_sock, buf, str_len); // 回显给客户端
                write(fds[1], buf, str_len);    // 写入管道（供日志进程读取并落盘）
            }

            // read 返回 0 表示客户端关闭连接（EOF）
            close(clnt_sock);
            puts("Client disconnected...");
            return 0; // 客户端子进程结束（触发 SIGCHLD，父进程回收）
        }
        else
            // 父进程不负责与该客户端通信，关闭 clnt_sock（连接由子进程持有并处理）
            close(clnt_sock);
    }

    // 正常情况下不会执行到这里（无限循环）
    close(serv_sock);
    return 0;
}

// -------------------- SIGCHLD 信号处理函数 --------------------
// 当任意子进程退出时，父进程会收到 SIGCHLD；在这里用 waitpid 回收一个子进程。
void read_childproc(int sig)
{
    int status;  // 保存子进程退出状态
    pid_t pid;

    // waitpid(-1, &status, WNOHANG)：
    // -1     ：回收任意一个子进程
    // WNOHANG：非阻塞；若没有已退出子进程则返回 0
    //
    // 注意：如果短时间内多个子进程退出，SIGCHLD 可能合并触发；
    // 更严谨的写法通常会用 while(waitpid(-1, &status, WNOHANG) > 0) 循环回收所有已退出子进程。
    pid = waitpid(-1, &status, WNOHANG);

    // 打印被回收的子进程 PID
    // 注意：在信号处理函数里调用 printf 并非严格的异步信号安全（示例演示用途）
    printf("Removed prod id: %d\n", pid);
}

void error_handling(char *message)
{
    // 输出错误信息到标准错误流 stderr
    fputs(message, stderr);
    fputc('\n', stderr);
    exit(1); // 非正常退出
}
