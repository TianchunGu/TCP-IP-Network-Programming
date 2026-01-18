#include <stdio.h>      // 标准输入输出：printf / fputs / fputc 等
#include <stdlib.h>     // 标准库：exit 等
#include <string.h>     // 内存/字符串操作：memset 等
#include <unistd.h>     // POSIX（本程序中未直接用到，但常与网络示例一起包含）
#include <arpa/inet.h>  // 网络地址转换：inet_addr / inet_ntoa 等
#include <netdb.h>      // DNS/主机信息查询：gethostbyaddr / struct hostent 等

void error_handling(char *message); // 错误处理函数声明：打印错误并退出

int main(int argc, char* argv[])
{
    struct hostent* host;       // 主机信息结构体指针（gethostbyaddr 返回）
    struct sockaddr_in addr;    // IPv4 地址结构体（这里主要用于存放输入的 IP）

    // 参数检查：程序只需要一个参数 —— IP 地址（点分十进制字符串）
    // 例如：./a.out 8.8.8.8
    if(argc != 2)
    {
        printf("Usage: %s <IP>\n", argv[0]);
        exit(1); // 参数不对直接退出
    }

    // 将 addr 清零，避免未初始化字段造成问题
    memset(&addr, 0, sizeof(addr));

    // inet_addr：把点分十进制 IP 字符串转换为网络字节序的 32 位 IPv4 地址
    // 注意：inet_addr 对非法输入会返回 INADDR_NONE（历史接口行为），实际使用中常用 inet_pton 更安全
    addr.sin_addr.s_addr = inet_addr(argv[1]);

    // gethostbyaddr：根据“二进制形式的 IP 地址”反查主机信息（反向解析，PTR 记录）
    // 参数说明：
    // (void*)&addr.sin_addr : 指向 IPv4 地址数据的指针（注意这里传的是 in_addr 结构体地址）
    // 4                    : 地址长度（IPv4 为 4 字节）
    // AF_INET               : 地址族为 IPv4
    //
    // 返回值：
    // 成功返回 struct hostent*，失败返回 NULL
    // 注意：gethostbyaddr 是较老的接口，线程安全性较差（常见替代：getnameinfo / getaddrinfo）
    host = gethostbyaddr((void*)&addr.sin_addr, 4, AF_INET);
    if(!host)
        error_handling("gethost... error"); // 反查失败（可能无 PTR 记录或 DNS 问题）则退出

    // h_name：官方主机名（canonical name）
    printf("Official name: %s\n", host->h_name);

    // h_aliases：主机别名列表（以 NULL 指针结尾的字符串数组）
    // 逐个打印别名
    for(int i = 0; host->h_aliases[i]; i++)
        printf("Aliases %d: %s\n", i+1, host->h_aliases[i]);

    // h_addrtype：地址类型（通常是 AF_INET 或 AF_INET6）
    // 本程序按示例只处理 AF_INET，但仍做了简单的输出分支
    printf("Address type: %s\n",
        (host->h_addrtype == AF_INET) ? "AF_INET":"AF_INET6");

    // h_addr_list：该主机对应的地址列表（以 NULL 结尾的指针数组）
    // 每个元素指向一个“网络字节序”的地址二进制数据
    // 对于 IPv4，可把它解释为 struct in_addr* 再用 inet_ntoa 转成字符串输出
    for(int i = 0; host->h_addr_list[i]; i++)
        printf("IP addr %d: %s\n", i+1,
            inet_ntoa(*(struct in_addr*)host->h_addr_list[i]));
            // 说明：
            // (struct in_addr*)host->h_addr_list[i] : 把地址数据指针转为 in_addr 指针
            // *(...)                                : 取出 in_addr 结构体
            // inet_ntoa                             : 将 IPv4 地址转为点分十进制字符串（返回静态缓冲区指针）

    return 0;
}

void error_handling(char *message)
{
    // 将错误信息写到标准错误流 stderr
	fputs(message, stderr);
	fputc('\n', stderr);
	exit(1); // 非正常退出
}
