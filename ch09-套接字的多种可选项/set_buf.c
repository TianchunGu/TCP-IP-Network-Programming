#include <stdio.h>      // 标准输入输出：printf / fputs / fputc 等
#include <stdlib.h>     // 标准库：exit 等
#include <unistd.h>     // POSIX（本程序中未直接用到，但常与 socket 示例一起包含）
#include <sys/socket.h> // 套接字 API：socket / setsockopt / getsockopt 以及 SOL_SOCKET 等常量

void error_handling(char *message); // 错误处理函数声明：打印错误信息并退出

int main(int argc, char* argv[])
{
    int sock;   // 套接字文件描述符（创建一个 TCP socket，用于演示设置/读取缓冲区大小）

    // 期望设置的缓冲区大小（单位：字节）
    // snd_buf：发送缓冲区，设为 100KB
    // rcv_buf：接收缓冲区，设为 10KB
    int snd_buf = 1024 * 100, rcv_buf = 1024 * 10;

    int state;        // 系统调用返回值（0 成功，-1 失败；此处用 if(state) 判断非 0 为错误）
    socklen_t len;    // getsockopt 的 optlen：输入/输出参数，表示缓冲区长度（字节数）

    // 创建 TCP 套接字（IPv4 + SOCK_STREAM）
    // 注意：这里只是演示 setsockopt/getsockopt，不需要 connect/bind
    sock = socket(PF_INET, SOCK_STREAM, 0);

    // -------------------- 设置接收缓冲区大小 SO_RCVBUF --------------------
    // setsockopt 参数说明：
    // sock              : 目标套接字
    // SOL_SOCKET        : socket 层级选项
    // SO_RCVBUF         : 接收缓冲区大小选项
    // (void*)&rcv_buf   : 指向选项值的指针（这里是 int）
    // sizeof(rcv_buf)   : 选项值长度（int 大小）
    //
    // 注意：
    // - 操作系统不一定严格按你给的值设置，可能会做最小/最大限制或按策略调整
    // - 有些系统会返回“加倍”的值（为协议开销预留），所以读回的值可能与设置值不同
    state = setsockopt(sock, SOL_SOCKET, SO_RCVBUF, (void*)&rcv_buf, sizeof(rcv_buf));
    if(state)
        error_handling("setsockopt() error");

    // -------------------- 设置发送缓冲区大小 SO_SNDBUF --------------------
    // SO_SNDBUF：发送缓冲区大小选项
    state = setsockopt(sock, SOL_SOCKET, SO_SNDBUF, (void*)&snd_buf, sizeof(snd_buf));
    if(state)
        error_handling("setsockopt() error");

    // -------------------- 读取发送缓冲区大小（验证实际生效值） --------------------
    // getsockopt 的最后一个参数是指针：&len
    // 调用前需把 len 设为可接收的缓冲区大小（这里为 sizeof(int)）
    len = sizeof(snd_buf);
    state = getsockopt(sock, SOL_SOCKET, SO_SNDBUF, (void*)&snd_buf, &len);
    if(state)
        error_handling("getsockopt() error");

    // -------------------- 读取接收缓冲区大小（验证实际生效值） --------------------
    len = sizeof(rcv_buf);
    state = getsockopt(sock, SOL_SOCKET, SO_RCVBUF, (void*)&rcv_buf, &len);
    if(state)
        error_handling("getsockopt() error");

    // 输出最终读回的缓冲区大小（可能与最初设置值不同，取决于内核策略）
    printf("Input buffer size: %d\n", rcv_buf);   // 接收缓冲区大小
    printf("Output buffer size: %d\n", snd_buf);  // 发送缓冲区大小

    return 0;
}

void error_handling(char *message)
{
    // 将错误信息输出到标准错误流 stderr
	fputs(message, stderr);
	fputc('\n', stderr);
	exit(1); // 非正常退出
}
