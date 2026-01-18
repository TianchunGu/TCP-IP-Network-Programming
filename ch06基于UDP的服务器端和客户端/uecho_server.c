#include <stdio.h>      // 标准输入输出：printf / fputs / fputc 等
#include <stdlib.h>     // 标准库：exit / atoi 等
#include <string.h>     // 字符串/内存操作：memset 等
#include <unistd.h>     // POSIX：close 等
#include <arpa/inet.h>  // 网络字节序转换：htonl / htons 等
#include <sys/socket.h> // 套接字 API：socket / bind / sendto / recvfrom 等

#define BUF_SIZE 30                 // 收发缓冲区大小（一次最多接收 30 字节）
void error_handling(char *message); // 错误处理函数声明：输出错误信息并退出

int main(int argc, char* argv[])
{
    int serv_sock;              // 服务器端 UDP 套接字（文件描述符）
    char message[BUF_SIZE];     // 用于接收/发送数据报的缓冲区
    int str_len;                // 实际接收到的数据长度（字节数）

    struct sockaddr_in serv_addr, clnt_addr; // serv_addr：服务器绑定地址；clnt_addr：客户端地址（用于回包）
    socklen_t clnt_addr_sz;                  // clnt_addr 结构体大小（recvfrom/sendto 需要）

    // 参数检查：服务器端只需要一个参数 —— 监听端口
    // argv[1] = port（如 9190）
    if(argc != 2)
    {
        printf("Usage: %s <port>\n", argv[0]);
        exit(1); // 参数不对直接退出
    }

    // 创建 UDP 套接字：
    // PF_INET    : IPv4
    // SOCK_DGRAM : UDP 数据报
    // 0          : 由系统选择 UDP 协议（IPPROTO_UDP）
    serv_sock = socket(PF_INET, SOCK_DGRAM, 0);
    if(serv_sock == -1)
        error_handling("socket() error"); // 创建失败则退出

    // 初始化服务器地址结构体为 0，避免未初始化字段带来问题
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;               // IPv4
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);// 绑定到本机所有网卡地址（0.0.0.0）
                                                   // INADDR_ANY 是主机字节序常量，需要 htonl 转网络字节序
    serv_addr.sin_port = htons(atoi(argv[1]));    // 设置监听端口：atoi 转 int，htons 转网络字节序

    // bind：将套接字与本地 IP/端口绑定，这样客户端才能把 UDP 数据报发到该端口
    // 若不 bind，服务器端将不知道监听哪个端口（通常会随机分配，不符合服务器需求）
    if(bind(serv_sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == -1)
        error_handling("bind() error");

    // 主循环：不断接收客户端数据，并把收到的数据原样发回（UDP 回显服务器）
    while (1)
    {
        // 准备接收：clnt_addr_sz 需要先设置为 clnt_addr 的大小
        // recvfrom 会把“发送方地址信息”写入 clnt_addr，并可能更新 clnt_addr_sz
        clnt_addr_sz = sizeof(clnt_addr);

        // recvfrom：接收一个 UDP 数据报
        // serv_sock                   : 监听套接字
        // message, BUF_SIZE           : 接收缓冲区与最大长度
        // 0                           : flags（通常为 0）
        // (struct sockaddr*)&clnt_addr: 输出参数，保存客户端地址（IP/端口）
        // &clnt_addr_sz               : 输入/输出参数，地址结构体长度
        // 返回值 str_len 为接收到的字节数（可能为 0，也可能小于 BUF_SIZE）
        str_len = recvfrom(serv_sock, message, BUF_SIZE, 0,
            (struct sockaddr*)&clnt_addr, &clnt_addr_sz);

        // sendto：把收到的数据原样发送回去（回显）
        // 注意：发送长度使用 str_len（刚收到的字节数），而不是 strlen
        // 因为 UDP 数据可能不是以 '\0' 结尾的字符串，也可能包含二进制数据
        sendto(serv_sock, message, str_len, 0,
            (struct sockaddr*)&clnt_addr, clnt_addr_sz);
    }

    // 关闭套接字（正常情况下这里不会执行到，因为 while(1) 无限循环）
    close(serv_sock);

    return 0;
}

void error_handling(char *message)
{
    // 将错误信息输出到标准错误流 stderr，便于与正常输出区分
    fputs(message, stderr);
    fputc('\n', stderr);
    exit(1); // 非正常退出
}
