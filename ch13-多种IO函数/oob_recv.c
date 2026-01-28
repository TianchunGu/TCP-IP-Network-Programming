#include <stdio.h>      // 标准输入输出：printf / puts / fputs / fputc 等
#include <unistd.h>     // POSIX：close / getpid 等
#include <stdlib.h>     // 标准库：exit / atoi 等
#include <string.h>     // 内存操作：memset 等
#include <signal.h>     // 信号处理：sigaction / sigemptyset / SIGURG 等
#include <sys/socket.h> // 套接字 API：socket / bind / listen / accept / recv 等
#include <netinet/in.h> // IPv4 地址结构：struct sockaddr_in / INADDR_ANY 等
#include <fcntl.h>      // fcntl：用于设置 socket 的“所有者进程”(F_SETOWN)

#define BUF_SIZE 30                 // 缓冲区大小：普通数据与紧急数据都使用该大小
void error_handling(char *message); // 错误处理函数：打印错误信息并退出
void urg_handler(int signo);        // SIGURG 信号处理函数：处理 TCP 紧急数据（OOB）

int serv_sock;  // 服务器监听套接字（全局变量，便于在不同函数中访问）
int clnt_sock;  // 与客户端建立连接后的套接字（全局变量：urg_handler 中需要使用）

int main(int argc, char *argv[])
{
    struct sockaddr_in serv_addr, clnt_addr; // serv_addr：服务器地址；clnt_addr：客户端地址（accept 填充）
    int str_len, state;                      // str_len：recv 返回的字节数；state：系统调用返回状态
    socklen_t clnt_addr_sz;                  // 客户端地址结构体长度
    struct sigaction act;                    // 信号处理配置结构体
    char buf[BUF_SIZE];                      // 普通数据接收缓冲区

    // -------------------- 参数检查 --------------------
    // 程序只需要一个参数：监听端口
    if (argc != 2)
    {
        printf("Usage : %s <port>\n", argv[0]);
        exit(1);
    }

    // -------------------- 配置信号处理：SIGURG --------------------
    // SIGURG：当 socket 收到“紧急数据”(Out-Of-Band, OOB) 时，内核会向“该 socket 的拥有者进程”发送 SIGURG。
    // 我们在这里把 SIGURG 的处理函数设置为 urg_handler，用来读取并打印紧急消息。
    act.sa_handler = urg_handler;     // 指定 SIGURG 到来时调用 urg_handler
    sigemptyset(&act.sa_mask);        // 清空屏蔽集：处理 SIGURG 时不额外屏蔽其他信号
    act.sa_flags = 0;                 // 不使用额外标志（例如 SA_RESTART 等）

    // -------------------- 创建 TCP 监听套接字 --------------------
    // PF_INET     : IPv4
    // SOCK_STREAM : TCP（面向连接的字节流）
    // 0           : 自动选择 TCP 协议（IPPROTO_TCP）
    serv_sock = socket(PF_INET, SOCK_STREAM, 0);

    // -------------------- 绑定本地地址与端口 --------------------
    memset(&serv_addr, 0, sizeof(serv_addr));      // 清零结构体
    serv_addr.sin_family = AF_INET;                // IPv4
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY); // 绑定到所有本地网卡地址（0.0.0.0）
    serv_addr.sin_port = htons(atoi(argv[1]));     // 端口：atoi 转 int，htons 转网络字节序

    // bind：绑定端口，使该 socket 能在指定端口上接受连接
    if (bind(serv_sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1)
        error_handling("bind() error");

    // listen：进入监听状态，backlog=5（等待队列上限，具体实现相关）
    listen(serv_sock, 5);

    // -------------------- accept：接受一个客户端连接 --------------------
    clnt_addr_sz = sizeof(clnt_addr);
    clnt_sock = accept(serv_sock, (struct sockaddr *)&clnt_addr, &clnt_addr_sz);
    // 注意：这里未检查 accept 返回值，真实工程应判断是否为 -1

    // -------------------- 设置 socket 的“所有者进程” --------------------
    // 为了让 SIGURG 信号能发送到本进程，需要指定该已连接 socket 的拥有者（owner）。
    // F_SETOWN：设置“接收 SIGURG 等异步通知的进程/进程组”
    // getpid()：当前进程 PID
    fcntl(clnt_sock, F_SETOWN, getpid());

    // -------------------- 安装 SIGURG 的信号处理器 --------------------
    // sigaction(SIGURG, &act, 0)：注册紧急信号处理函数 urg_handler
    state = sigaction(SIGURG, &act, 0);
    // 注意：示例未检查 state 是否为 -1；真实工程应检查是否安装成功

    // -------------------- 主循环：接收普通数据 --------------------
    // recv(clnt_sock, buf, sizeof(buf), 0)：
    // - 0 表示普通接收（不带 MSG_OOB）
    // 返回值 str_len：
    // - >0：收到的字节数
    // - 0 ：对端关闭连接（EOF），循环结束
    // - -1：出错（例如被信号中断 EINTR 等）
    while ((str_len = recv(clnt_sock, buf, sizeof(buf), 0)) != 0)
    {
        // 若 recv 返回 -1，这里直接 continue（跳过本次处理）
        // 常见原因：系统调用被信号打断（例如 SIGURG 到来时可能中断 recv）
        if (str_len == -1)
            continue;

        // 将收到的数据当作 C 字符串打印时，需要补 '\0'
        // 注意：如果 str_len == sizeof(buf)，则 buf[str_len] 会越界；
        // 更稳妥写法通常用 sizeof(buf)-1 读取，或根据返回值做边界保护。
        buf[str_len] = 0;

        // 输出普通消息
        puts(buf);
    }

    // -------------------- 关闭套接字 --------------------
    close(clnt_sock);
    close(serv_sock);
    return 0;
}

// -------------------- SIGURG 处理函数：读取 TCP 紧急数据（OOB） --------------------
void urg_handler(int signo)
{
    int str_len;
    char buf[BUF_SIZE];

    // recv(..., MSG_OOB)：接收紧急数据（Out-Of-Band）
    // - MSG_OOB 标志告诉内核读取“紧急指针”标记的数据
    // - 这里只读 sizeof(buf)-1，预留 '\0' 位置
    //
    // 注意：
    // - TCP 的“紧急数据”语义在不同实现中是“紧急指针之前的最后一个字节”，并非真正独立通道；
    //   但教学示例通常用它来演示 OOB 与 SIGURG 的结合。
    str_len = recv(clnt_sock, buf, sizeof(buf) - 1, MSG_OOB);

    // 手动补 '\0'，将其当作字符串输出
    buf[str_len] = 0;

    // 打印紧急消息
    // 注意：严格来说在信号处理函数里调用 printf 不是异步信号安全的（示例演示用途）
    printf("Urgent message: %s \n", buf);
}

void error_handling(char *message)
{
    // 输出错误信息到标准错误流 stderr
    fputs(message, stderr);
    fputc('\n', stderr);
    exit(1); // 非正常退出
}
