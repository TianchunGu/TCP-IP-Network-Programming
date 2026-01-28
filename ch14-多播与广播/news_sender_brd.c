#include <stdio.h>      // 标准输入输出：printf / fputs / fputc / FILE / fopen / fgets 等
#include <stdlib.h>     // 标准库：exit / atoi 等
#include <string.h>     // 字符串/内存操作：memset / strlen 等
#include <unistd.h>     // POSIX：close / sleep 等
#include <arpa/inet.h>  // 网络地址与字节序：inet_addr / htons 等
#include <sys/socket.h> // 套接字 API：socket / setsockopt / sendto 等

#define BUF_SIZE 30                 // 每次从文件读取并发送的缓冲区大小
void error_handling(char *message); // 错误处理函数：打印错误信息并退出

int main(int argc, char *argv[])
{
    int send_sock;                      // UDP 套接字：用于发送广播数据报
    struct sockaddr_in broad_adr;       // 广播目标地址结构体（IPv4）：包含广播 IP 与端口
    FILE *fp;                           // 文件指针：读取要发送的内容
    char buf[BUF_SIZE];                 // 发送缓冲区：存放从文件读出的每一行/片段
    int so_brd = 1;                     // SO_BROADCAST 选项开关（1=允许广播，0=禁止）

    // -------------------- 参数检查 --------------------
    // 需要两个参数：
    // argv[1] = 广播 IP（例如 255.255.255.255 或子网定向广播地址如 192.168.1.255）
    // argv[2] = 端口
    if (argc != 3)
    {
        printf("Usage : %s <Boradcast IP> <PORT>\n", argv[0]);
        exit(1);
    }

    // -------------------- 创建 UDP 套接字 --------------------
    // PF_INET    : IPv4
    // SOCK_DGRAM : UDP（无连接的数据报）
    // 0          : 自动选择 UDP 协议（IPPROTO_UDP）
    send_sock = socket(PF_INET, SOCK_DGRAM, 0);

    // -------------------- 配置广播目标地址 --------------------
    memset(&broad_adr, 0, sizeof(broad_adr));         // 清零结构体
    broad_adr.sin_family = AF_INET;                   // IPv4
    broad_adr.sin_addr.s_addr = inet_addr(argv[1]);   // 广播 IP 字符串 -> 网络字节序地址
    broad_adr.sin_port = htons(atoi(argv[2]));        // 端口转网络字节序

    // -------------------- 允许广播：设置 SO_BROADCAST --------------------
    // 默认情况下，许多系统不允许向广播地址发送 UDP 数据报；
    // 必须显式设置 SO_BROADCAST=1 才能 sendto 到广播地址，否则可能返回权限/不可达等错误。
    //
    // setsockopt 参数含义：
    // - send_sock            ：要设置的 socket
    // - SOL_SOCKET           ：socket 层选项
    // - SO_BROADCAST         ：允许发送广播
    // - &so_brd, sizeof(...) ：选项值与长度
    int state = setsockopt(send_sock, SOL_SOCKET,
               SO_BROADCAST, (void *)&so_brd, sizeof(so_brd));
    if(state == -1)
        error_handling("setsockopt() error");

    // -------------------- 打开要发送的文件 --------------------
    // 从 news.txt 读取内容，并分批发送到广播地址
    if ((fp = fopen("news.txt", "r")) == NULL)
        error_handling("fopen() error");

    // -------------------- 循环读取并广播发送 --------------------
    // !feof(fp) 是教学示例中常见写法：
    // 注意：严格写法通常是 while(fgets(buf, BUF_SIZE, fp)) 来避免最后一次读取失败仍进入循环。
    while (!feof(fp))
    {
        // 从文件读取一行（最多 BUF_SIZE-1 字符），并自动补 '\0'
        // 若一行长度超过 BUF_SIZE-1，会被分段读取并分段发送
        fgets(buf, BUF_SIZE, fp);

        // sendto：向指定地址发送 UDP 数据报
        // - buf         ：发送的数据
        // - strlen(buf) ：发送长度（不包含 '\0'）
        // - 0           ：无特殊标志
        // - &broad_adr  ：广播目标地址（IP + 端口）
        sendto(send_sock, buf, strlen(buf),
               0, (struct sockaddr *)&broad_adr, sizeof(broad_adr));

        // 每次发送后暂停 2 秒：
        // - 便于演示观察广播消息逐条到达
        // - 避免发送过快（演示用途）
        sleep(2);
    }

    // -------------------- 关闭套接字 --------------------
    close(send_sock);
    return 0;
}

void error_handling(char *message)
{
    // 输出错误信息到标准错误流 stderr
    fputs(message, stderr);
    fputc('\n', stderr);
    exit(1); // 非正常退出
}
