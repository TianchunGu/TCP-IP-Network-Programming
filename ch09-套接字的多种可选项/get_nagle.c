#include <stdio.h>      // 标准输入输出：printf / fputs / fputc 等
#include <stdlib.h>     // 标准库：exit 等
#include <string.h>     // 字符串/内存操作（本程序未直接用到，但网络示例中常包含）
#include <unistd.h>     // POSIX（本程序未直接用到，但常与 socket 示例一并包含）
#include <arpa/inet.h>  // 网络相关（本程序未直接用到，但常见于网络代码模板）
#include <sys/socket.h> // 套接字 API：socket / getsockopt / setsockopt 等
#include <netinet/tcp.h>// TCP 相关选项：TCP_NODELAY 等（禁用 Nagle 算法）

void error_handling(char *message); // 错误处理函数声明：打印错误信息并退出

int main(int argc, char* argv[])
{
    int sock;                 // 套接字文件描述符（这里创建一个 TCP socket 用于查询/设置 TCP 选项）
    int state;                // 保存系统调用返回状态（0 表示成功，-1 表示失败）
    int opt_val;              // 用于保存/设置 socket 选项的值（TCP_NODELAY 为 0/1）
    socklen_t opt_len = sizeof(opt_val); // 选项值长度：getsockopt/setsockopt 需要该长度信息

    // 创建一个 TCP 套接字（IPv4 + SOCK_STREAM）
    // 注意：本程序仅演示选项的读取/设置，不需要 bind/connect
    sock = socket(PF_INET, SOCK_STREAM, 0);

    // -------------------- 读取 TCP_NODELAY 当前值 --------------------
    // getsockopt：读取套接字选项
    // sock                      : 目标套接字
    // IPPROTO_TCP               : 选项所在的协议层级（TCP 层）
    // TCP_NODELAY               : 具体选项名（禁用/启用 Nagle 算法）
    // (void*)&opt_val           : 输出缓冲区地址，用来接收选项值
    // &opt_len                  : 输入/输出参数：输入时为缓冲区大小，输出时为实际写入长度
    //
    // TCP_NODELAY 含义：
    // - opt_val = 0：默认行为（Nagle 算法可能启用，具体与系统实现有关）
    // - opt_val = 1：禁用 Nagle 算法（通常用于降低小包发送延迟，但可能增加网络包数量）
    state = getsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (void*)&opt_val, &opt_len);
    if(state)
        error_handling("getsockopt() error"); // 读取失败则退出

    // 打印当前 TCP_NODELAY 值
    printf("TCP_NODELAY: %d\n", opt_val);

    // -------------------- 设置 TCP_NODELAY = 1（禁用 Nagle） --------------------
    opt_val = 1; // 期望设置为 1：禁用 Nagle 算法，减少小数据发送延迟

    // setsockopt：设置套接字选项
    // 与 getsockopt 不同，setsockopt 的最后一个参数是值长度（不是指针）
    state = setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (void*)&opt_val, opt_len);
    if(state)
        error_handling("setsockopt() error"); // 设置失败则退出

    // -------------------- 再次读取 TCP_NODELAY，验证设置是否生效 --------------------
    state = getsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (void*)&opt_val, &opt_len);
    if(state)
        error_handling("getsockopt() error");

    // 输出设置后的值
    printf("After setting, the value of TCP_NODELAY is: %d\n", opt_val);

    return 0;
}

void error_handling(char *message)
{
    // 将错误信息输出到标准错误流 stderr
	fputs(message, stderr);
	fputc('\n', stderr);
	exit(1); // 非正常退出
}
