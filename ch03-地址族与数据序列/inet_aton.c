#include <stdio.h>     // printf, fputs, stderr
#include <stdlib.h>    // exit
#include <arpa/inet.h> // inet_aton, sockaddr_in

// 统一错误处理：输出错误信息并终止程序
void error_handling(char *message);

int main(int argc, char* argv[])
{
    char *addr = "123.124.234.222";   // 点分十进制 IP 字符串
    struct sockaddr_in addr_inet;     // IPv4 地址结构体

    // inet_aton 成功返回 1，失败返回 0
    if(!inet_aton(addr, &addr_inet.sin_addr))
        error_handling("inet_aton() error!");
    else
        // 输出网络字节序的整数形式地址
        printf("Newwork ordered integer addr: %#x\n", addr_inet.sin_addr.s_addr);

    return 0;
}

void error_handling(char* message)
{
    // 将错误信息输出到标准错误并终止程序
    fputs(message, stderr);
    fputc('\n', stderr);
    exit(1);
}
