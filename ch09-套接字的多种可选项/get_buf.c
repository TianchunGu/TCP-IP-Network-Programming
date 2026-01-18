#include <stdio.h>      // 标准输入输出：printf / fputs / fputc 等
#include <stdlib.h>     // 标准库：exit 等
#include <unistd.h>     // POSIX（本程序中未直接用到，但常与网络示例一起包含）
#include <sys/socket.h> // 套接字 API：socket / getsockopt 以及 SOL_SOCKET/SO_SNDBUF/SO_RCVBUF 等常量

void error_handling(char *message); // 错误处理函数声明：打印错误信息并退出

int main(int argc, char* argv[])
{
    int sock;                 // 套接字文件描述符（这里创建一个 TCP 套接字用于查询其缓冲区属性）
    int snd_buf, rcv_buf;     // snd_buf：发送缓冲区大小；rcv_buf：接收缓冲区大小（单位：字节）
    int state;                // 保存系统调用返回状态（0 表示成功，-1 表示失败）
    socklen_t len;            // getsockopt 的 optlen 参数：传入/传出“缓冲区长度”

    // 创建一个 TCP 套接字（IPv4 + SOCK_STREAM）
    // 注意：这里只是为了查询 socket 选项，并不需要 connect/bind
    sock = socket(PF_INET, SOCK_STREAM, 0);

    // -------------------- 查询发送缓冲区大小 SO_SNDBUF --------------------
    // len 表示我们提供的存储空间大小（字节数），getsockopt 会根据它写入实际返回值
    // 对于 int 类型选项，一般传 sizeof(int)
    len = sizeof(snd_buf);

    // getsockopt：读取套接字选项
    // sock                 : 目标套接字
    // SOL_SOCKET           : 选项层级（socket 层级通用选项）
    // SO_SNDBUF            : 具体选项（发送缓冲区大小）
    // (void*)&snd_buf      : 输出缓冲区地址，用于接收返回的选项值
    // &len                 : 输入/输出参数：输入时表示缓冲区大小，输出时表示实际写入的长度
    state = getsockopt(sock, SOL_SOCKET, SO_SNDBUF, (void*)&snd_buf, &len);
    if(state)
        error_handling("getsockopt() error"); // 获取失败则退出

    // -------------------- 查询接收缓冲区大小 SO_RCVBUF --------------------
    // 同样先设置 len 为 rcv_buf 的大小
    len = sizeof(rcv_buf);

    // SO_RCVBUF：接收缓冲区大小
    state = getsockopt(sock, SOL_SOCKET, SO_RCVBUF, (void*)&rcv_buf, &len);
    if(state)
        error_handling("getsocketopt() error"); // 注意：这里字符串写成 getsocketopt，但不影响代码逻辑

    // 输出查询到的缓冲区大小
    // rcv_buf：接收缓冲区（Input buffer）
    // snd_buf：发送缓冲区（Output buffer）
    //
    // 说明：
    // - 不同操作系统对 SO_SNDBUF/SO_RCVBUF 的含义和返回值细节可能不同（例如可能是内核实际分配值）
    // - 有些系统会返回“加倍”的值（为协议开销预留），因此显示值不一定等于你设置的值
    printf("Input buffer size: %d\n", rcv_buf);
    printf("Output buffer size: %d\n", snd_buf);

    return 0;
}

void error_handling(char *message)
{
    // 将错误信息输出到标准错误流 stderr
	fputs(message, stderr);
	fputc('\n', stderr);
	exit(1); // 非正常退出
}
