#include <stdio.h>      // 标准输入输出：printf / fputs / fputc 等
#include <stdlib.h>     // 标准库：exit / atoi 等
#include <string.h>     // 内存/字符串操作：memset 等
#include <unistd.h>     // POSIX I/O：read / write / close 等
#include <arpa/inet.h>  // 网络字节序与地址：htonl / htons 等
#include <sys/socket.h> // 套接字 API：socket / bind / listen / accept / setsockopt 等

#define TRUE 1          // 自定义布尔真（用于 setsockopt 选项）
#define FALSE 0         // 自定义布尔假（本程序未使用，但常配套定义）
void error_handling(char *message); // 错误处理函数声明：打印错误信息并退出

int main(int argc, char* argv[])
{
    int serv_sock, clnt_sock;              // serv_sock：监听套接字；clnt_sock：与某个客户端建立连接后的套接字
    char message[30];                      // 收发缓冲区（用于回显），大小 30 字节
    int option, str_len;                   // option：套接字选项值；str_len：read 实际读取的字节数
    struct sockaddr_in serv_addr, clnt_addr; // serv_addr：服务器地址；clnt_addr：客户端地址（accept 填充）
    socklen_t clnt_addr_sz, optlen;        // clnt_addr_sz：客户端地址结构体长度；optlen：setsockopt 参数长度

    // 参数检查：服务器端只需要一个参数 —— 监听端口
    // argv[1] = port（如 9190）
    if(argc != 2)
    {
        printf("Usage: %s <port>\n", argv[0]);
        exit(1); // 参数不正确直接退出
    }

    // 创建 TCP 监听套接字：
    // PF_INET     : IPv4
    // SOCK_STREAM : TCP（面向连接的字节流）
    // 0           : 让系统自动选择 TCP 协议（IPPROTO_TCP）
    serv_sock = socket(PF_INET, SOCK_STREAM, 0);
    if(serv_sock == -1)
        error_handling("socket() error"); // 创建失败则退出

    // -------------------- 设置套接字选项：SO_REUSEADDR --------------------
    // SO_REUSEADDR 的目的：
    // - 允许快速重启服务器并重新绑定到同一端口
    // - 避免因 TIME_WAIT 等原因导致 bind() 失败（典型报错：Address already in use）
    // 注意：这并不表示“允许多个进程同时 listen 同一端口”（那是 SO_REUSEPORT 等语义，且与系统有关）
    option = TRUE;             // 开启该选项
    optlen = sizeof(option);   // 选项值长度（int 大小）
    setsockopt(serv_sock, SOL_SOCKET, SO_REUSEADDR, (void*)&option, optlen);
    // 这里没有检查返回值；真实工程中建议判断 setsockopt 是否失败并处理

    // 初始化服务器地址结构体
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;               // IPv4
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);// 绑定到所有本地网卡地址（0.0.0.0）
    serv_addr.sin_port = htons(atoi(argv[1]));    // 监听端口：atoi 转 int，htons 转网络字节序

    // bind：将监听套接字绑定到本地 IP/端口，使其可以接收该端口上的连接请求
    if(bind(serv_sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == -1)
        error_handling("bind() error");

    // listen：把套接字变成“被动监听”状态，开始接收 TCP 连接请求
    // 5：backlog，表示已完成连接（或半连接）的等待队列长度上限（具体行为依赖系统实现）
    if(listen(serv_sock, 5) == -1)
        error_handling("listen() error");

    // accept：从监听队列中取出一个已完成握手的连接，返回一个新的“已连接套接字”
    // serv_sock 仍用于继续监听；clnt_sock 用于与当前客户端收发数据
    clnt_addr_sz = sizeof(clnt_addr); // 传入 clnt_addr 的缓冲区大小，accept 会写入客户端地址并更新长度
    clnt_sock = accept(serv_sock, (struct sockaddr*)&clnt_addr, &clnt_addr_sz);
    if(clnt_sock == -1)
        error_handling("accept() error");

    // 回显循环：
    // read 从 clnt_sock 读取客户端发来的字节流，返回值：
    // - >0：成功读取到的字节数
    // - 0 ：对端关闭连接（EOF），循环结束
    // - -1：出错（本程序未显式处理 -1，真实工程应判断并处理）
    while((str_len = read(clnt_sock, message, sizeof(message))) != 0)
    {
        // 将收到的数据原样写回给客户端（echo）
        write(clnt_sock, message, str_len);

        // 同时将收到的数据写到标准输出（文件描述符 1）
        // 1 表示 stdout（等价于 STDOUT_FILENO），直接用 write 绕过 stdio 缓冲
        write(1, message, str_len);
    }

    // 关闭与客户端通信的套接字（释放资源，并触发 TCP 连接关闭流程）
    close(clnt_sock);

    return 0;
}

void error_handling(char *message)
{
    // 输出错误信息到标准错误流 stderr
	fputs(message, stderr);
	fputc('\n', stderr);
	exit(1); // 非正常退出
}
