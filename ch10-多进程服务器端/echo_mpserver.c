#include <stdio.h>      // 标准输入输出：printf / puts / fputs / fputc 等
#include <stdlib.h>     // 标准库：exit / atoi 等
#include <string.h>     // 字符串/内存操作：memset 等
#include <unistd.h>     // POSIX：read / write / close / fork 等
#include <signal.h>     // 信号处理：sigaction / sigemptyset / SIGCHLD 等
#include <sys/wait.h>   // 进程回收：waitpid / WNOHANG 等
#include <arpa/inet.h>  // 网络字节序：htonl / htons 等
#include <sys/socket.h> // 套接字 API：socket / bind / listen / accept 等

#define BUF_SIZE 30                 // 回显缓冲区大小（一次最多收/发 30 字节）
void error_handling(char *message); // 错误处理函数：打印错误并退出
void read_childproc(int sig);       // SIGCHLD 信号处理函数：回收子进程，避免僵尸进程

int main(int argc, char* argv[])
{
    int serv_sock, clnt_sock;                 // serv_sock：监听套接字；clnt_sock：与某个客户端连接后的套接字
    struct sockaddr_in serv_addr, clnt_addr;  // serv_addr：服务器地址；clnt_addr：客户端地址（accept 填充）
    socklen_t addr_sz;                        // 客户端地址结构体长度（accept 需要）

    pid_t pid;              // fork 返回值：区分父/子进程
    struct sigaction act;   // 设置信号处理行为的结构体
    int state, str_len;     // state：系统调用状态；str_len：read 返回的实际读取字节数
    char buf[BUF_SIZE];     // 收发缓冲区：用于回显

    // -------------------- 参数检查 --------------------
    // 服务器程序只需要一个参数：监听端口
    // argv[1] = port（例如 9190）
    if(argc != 2)
    {
        printf("Usage: %s <port>\n", argv[0]);
        exit(1);
    }

    // -------------------- 注册 SIGCHLD 处理函数 --------------------
    // 当子进程退出时，父进程会收到 SIGCHLD 信号。
    // 若父进程不 wait/waitpid 回收子进程，子进程会变成僵尸进程（占用进程表项）。
    act.sa_handler = read_childproc;   // 指定信号处理函数
    sigemptyset(&act.sa_mask);         // 清空信号屏蔽集：处理该信号时不额外屏蔽其他信号
    act.sa_flags = 0;                  // 标志位（此处不使用 SA_RESTART 等）
    state = sigaction(SIGCHLD, &act, 0); // 安装 SIGCHLD 的处理动作
    // 注意：这里没有检查 state 是否为 -1；真实工程应判断 sigaction 是否成功

    // -------------------- 创建监听套接字 --------------------
    // PF_INET     : IPv4
    // SOCK_STREAM : TCP（面向连接）
    // 0           : 让系统自动选择 TCP 协议（IPPROTO_TCP）
    serv_sock = socket(PF_INET, SOCK_STREAM, 0);

    // -------------------- 填充并绑定服务器地址 --------------------
    memset(&serv_addr, 0, sizeof(serv_addr));      // 清零结构体
    serv_addr.sin_family = AF_INET;                // IPv4
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY); // 绑定到所有本地网卡地址（0.0.0.0）
    serv_addr.sin_port = htons(atoi(argv[1]));     // 端口号转网络字节序

    // bind：绑定本地 IP/端口，使服务器在该端口上监听连接
    if(bind(serv_sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == -1)
        error_handling("bind() error");

    // listen：进入监听状态，backlog=5 表示等待队列上限（具体行为依赖系统实现）
    if(listen(serv_sock, 5) == -1)
        error_handling("listen() error");

    // -------------------- 主循环：不断 accept + fork 处理客户端 --------------------
    while(1)
    {
        // accept 会阻塞等待新的客户端连接；成功后返回“已连接套接字” clnt_sock
        addr_sz = sizeof(clnt_addr);
        clnt_sock = accept(serv_sock, (struct sockaddr*)&clnt_addr, &addr_sz);

        // accept 失败返回 -1；可能被信号中断（例如 SIGCHLD）或其他原因
        // 本例中失败则 continue，继续下一轮 accept
        if(clnt_sock == -1)
            continue;
        else
            puts("New client connected...");

        // fork：为该客户端创建子进程处理
        pid = fork();

        // fork 失败：关闭刚接收的 clnt_sock，继续接收下一个连接
        if(pid == -1)
        {
            close(clnt_sock);
            continue;
        }

        // -------------------- 子进程逻辑：负责与该客户端通信 --------------------
        if(pid == 0)
        {
            // 子进程不需要监听套接字（serv_sock），关闭以减少资源占用
            // 同时避免父/子都持有监听套接字导致的混乱
            close(serv_sock);

            // 回显循环：读取客户端数据并原样写回
            // read 返回：
            // - >0：读取到的字节数
            // - 0 ：客户端关闭连接（EOF），循环结束
            // - -1：出错（本示例未单独处理负值）
            while((str_len = read(clnt_sock, buf, BUF_SIZE)) != 0)
                write(clnt_sock, buf, str_len); // 将收到的数据回显给客户端

            // 客户端断开后关闭连接套接字
            close(clnt_sock);
            puts("Client disconnected...");

            // 子进程结束（return 0 会导致进程退出），触发父进程收到 SIGCHLD
            return 0;
        }
        // -------------------- 父进程逻辑：继续 accept 其他客户端 --------------------
        else
            // 父进程不负责与该客户端通信，因此关闭 clnt_sock
            // 该连接由子进程持有并处理
            close(clnt_sock);
    }

    // 正常情况下不会执行到这里（无限循环）
    close(serv_sock);
    return 0;
}

void read_childproc(int sig)
{
    pid_t pid;
    int status;

    // waitpid(-1, ...)：
    // -1 表示等待“任意子进程”
    // WNOHANG 表示“非阻塞”：如果没有已退出的子进程，立即返回 0
    //
    // 在 SIGCHLD 处理函数中使用 WNOHANG 可以避免在信号处理期间阻塞
    pid = waitpid(-1, &status, WNOHANG);

    // 打印被回收的子进程 PID
    // 注意：严格来说，在信号处理函数里调用 printf 不是异步信号安全的（示例代码为了演示）
    printf("Removed proc id: %d\n", pid);
}

void error_handling(char *message)
{
    // 输出错误信息到标准错误流 stderr
    fputs(message, stderr);
    fputc('\n', stderr);
    exit(1); // 非正常退出
}
