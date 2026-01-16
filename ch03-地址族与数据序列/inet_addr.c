#include <stdio.h>     // printf
#include <arpa/inet.h> // inet_addr, INADDR_NONE

int main(int argc, char *argv[])
{
    char *addr1 = "127.168.198.111"; // 合法 IP 地址
    char *addr2 = "127.199.198.256"; // 非法 IP 地址（256 超出范围）

    // inet_addr 失败返回 INADDR_NONE
    unsigned long conv_addr = inet_addr(addr1);
    if(conv_addr == INADDR_NONE)
        printf("Error Occured!\n");
    else
        // 输出网络字节序的整数形式地址
        printf("NewWork Byte Order Integer Address: %#lx\n", conv_addr);
    
    // 对非法 IP 地址进行转换，预期失败
    conv_addr = inet_addr(addr2);
    if(conv_addr == INADDR_NONE)
        printf("Error Occured!\n");
    else
        printf("NewWork Byte Order Integer Address: %#lx\n", conv_addr);

    return 0;
}
