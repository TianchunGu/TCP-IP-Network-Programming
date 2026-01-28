#include <stdio.h>      // 标准输入输出：printf / fputs / fputc / FILE / fopen / fgets / fclose 等
#include <stdlib.h>     // 标准库：exit / atoi 等
#include <string.h>     // 字符串操作：memset / strlen 等
#include <unistd.h>     // POSIX：close / sleep 等
#include <arpa/inet.h>  // 网络地址与字节序：inet_addr / htons 等
#include <sys/socket.h> // 套接字 API：socket / setsockopt / sendto 等

#define TTL 64                  // 组播 TTL（Time To Live）：IP 数据报可经过的最大路由跳数
#define BUF_SIZE 30             // 每次从文件读取并发送的缓冲区大小
void error_handling(char* message); // 错误处理函数声明：打印错误并退出

int main(int argc, char* argv[])
{
    int send_sock;                      // UDP 套接字：用于向组播地址发送数据
    struct sockaddr_in mul_addr;        // 组播目标地址（IPv4）：包含组播 IP 与端口
    int time_live = TTL;                // 组播 TTL 的设置值（用于 setsockopt）
    FILE* fp;                           // 文件指针：用于读取要发送的内容
    char buf[BUF_SIZE];                 // 发送缓冲区：存放从文件中读出的每一行/片段

    // -------------------- 参数检查 --------------------
    // 需要两个参数：
    // argv[1] = 组播 IP（例如 224.0.0.1 / 239.x.x.x 等）
    // argv[2] = 端口（例如 9190）
    if(argc != 3)
    {
        printf("Usage: %s <GroupIP> <PORT>\n", argv[0]);
        exit(1);
    }

    // -------------------- 创建 UDP 套接字 --------------------
    // PF_INET    : IPv4
    // SOCK_DGRAM : UDP（无连接的数据报）
    // 0          : 自动选择 UDP 协议（IPPROTO_UDP）
    send_sock = socket(PF_INET, SOCK_DGRAM, 0);

    // -------------------- 配置组播目标地址 --------------------
    memset(&mul_addr, 0, sizeof(mul_addr));        // 清零结构体
    mul_addr.sin_family = AF_INET;                 // IPv4
    mul_addr.sin_addr.s_addr = inet_addr(argv[1]); // 组播 IP 字符串 -> 网络字节序地址
    mul_addr.sin_port = htons(atoi(argv[2]));      // 端口：atoi 转 int，htons 转网络字节序

    // -------------------- 设置组播 TTL --------------------
    // setsockopt(send_sock, IPPROTO_IP, IP_MULTICAST_TTL, ...)：
    // - IPPROTO_IP         ：设置 IP 层选项
    // - IP_MULTICAST_TTL   ：设置组播数据报的 TTL（生存时间/最大跳数）
    // - time_live=64       ：允许组播包最多经过 64 个路由跳数（大致控制传播范围）
    //
    // 说明：
    // - TTL 越大，组播包可能传播得越远；越小则限制在较近范围
    // - 组播的实际传播还取决于网络设备/路由配置与是否允许组播转发
    setsockopt(send_sock, IPPROTO_IP, IP_MULTICAST_TTL,
        (void*)&time_live, sizeof(time_live));

    // -------------------- 打开要广播的文件 --------------------
    // 打开 news.txt，按文本读方式读取
    // 该文件中的内容将分批发送到组播地址
    if((fp = fopen("news.txt", "r")) == NULL)
        error_handling("fopen() error");

    // -------------------- 循环读取文件并发送 --------------------
    // !feof(fp) 常见于教学示例：表示“尚未到达 EOF”
    // 注意：严格写法通常是 while(fgets(...))，避免最后一次读失败仍进入循环的问题
    while(!feof(fp))
    {
        // fgets：从文件读取一行（最多 BUF_SIZE-1 个字符），并自动补 '\0'
        // 若一行超过 BUF_SIZE-1，会分多次读取并发送（截断/分片）
        fgets(buf, BUF_SIZE, fp);

        // sendto：向指定目标地址发送 UDP 数据报
        // send_sock ：发送 socket
        // buf       ：要发送的数据
        // strlen(buf)：发送长度（不包含 '\0'）
        // 0         ：无特殊标志
        // &mul_addr ：目标地址（组播 IP + 端口）
        sendto(send_sock, buf, strlen(buf), 0,
            (struct sockaddr*)&mul_addr, sizeof(mul_addr));

        // 每发送一段内容暂停 2 秒：
        // - 让接收端更容易观察到分批到来的消息
        // - 避免发送过快导致输出刷屏或网络拥塞（演示用途）
        sleep(2);
    }

    // -------------------- 清理资源 --------------------
    fclose(fp);        // 关闭文件
    close(send_sock);  // 关闭套接字

    return 0;
}

void error_handling(char *message)
{
    // 输出错误信息到标准错误流 stderr
	fputs(message, stderr);
	fputc('\n', stderr);
	exit(1); // 非正常退出
}
