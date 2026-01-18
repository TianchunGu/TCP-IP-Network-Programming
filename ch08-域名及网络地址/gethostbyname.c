#include <stdio.h>      // 标准输入输出：printf / fputs / fputc 等
#include <stdlib.h>     // 标准库：exit 等
#include <unistd.h>     // POSIX（本程序中未直接使用，但常与网络示例一并包含）
#include <arpa/inet.h>  // 网络地址转换：inet_ntoa 等（把二进制 IPv4 转字符串）
#include <netdb.h>      // 主机信息查询：gethostbyname / struct hostent 等

void error_handling(char *message); // 错误处理函数声明：输出错误信息并退出

int main(int argc, char* argv[])
{
    struct hostent *host; // 保存主机查询结果的结构体指针（gethostbyname 返回）

    // 参数检查：需要一个参数 <addr>
    // <addr> 通常是域名（如 www.example.com），有些实现也允许直接输入 IPv4 字符串
    if(argc != 2)
    {
        printf("Usage: %s <addr>\n", argv[0]);
        exit(1); // 参数不正确直接退出
    }

    // gethostbyname：根据主机名（域名）查询主机信息（正向解析）
    // 返回 struct hostent*，失败返回 NULL
    // 注意：
    // 1) 这是较老的接口，线程安全性较差（常见替代：getaddrinfo）
    // 2) 只适用于 IPv4（或在部分系统上对 IPv6 支持有限），现代程序更推荐 getaddrinfo
    host = gethostbyname(argv[1]);
    if(!host)
        error_handling("gethost... error"); // 解析失败（DNS 问题、域名不存在等）则退出

    // h_name：官方主机名（canonical name）
    printf("official name: %s\n", host->h_name);

    // h_aliases：主机别名列表（以 NULL 结尾的字符串指针数组）
    // 逐个输出别名
    for(int i = 0; host->h_aliases[i]; i++)
        printf("Aliases %d: %s\n", i+1, host->h_aliases[i]);

    // h_addrtype：地址类型（通常为 AF_INET 或 AF_INET6）
    // 这里仅做输出显示
    printf("Address type: %s\n",
        (host->h_addrtype == AF_INET) ? "AF_INET" : "AF_INET6");

    // h_addr_list：地址列表（以 NULL 结尾的指针数组）
    // 每个元素指向一个网络字节序的二进制地址
    //
    // 对于 IPv4：
    // - 可以把 host->h_addr_list[i] 解释为 struct in_addr*
    // - 再用 inet_ntoa 转换为点分十进制字符串输出
    //
    // 注意：inet_ntoa 返回的是指向静态缓冲区的指针，后续调用会覆盖之前的结果
    for(int i = 0; host->h_addr_list[i]; i++)
        printf("IP addr %d: %s\n", i + 1, inet_ntoa(*(struct in_addr*)host->h_addr_list[i]));
                    // 解释：
                    // (struct in_addr*)host->h_addr_list[i] : 将地址数据指针转为 in_addr 指针
                    // *(...)                                 : 取出 in_addr 结构体
                    // inet_ntoa                              : 转成 "x.x.x.x" 形式字符串

    return 0;
}

void error_handling(char *message)
{
    // 输出错误信息到标准错误流 stderr
	fputs(message, stderr);
	fputc('\n', stderr);
	exit(1); // 非正常退出
}
