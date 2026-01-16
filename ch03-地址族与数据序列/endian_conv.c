#include <stdio.h>     // printf
#include <arpa/inet.h> // htons, htonl

int main(int argc, char* argv[])
{
    unsigned short host_port = 0x1234;   // 主机字节序端口
    unsigned short net_port;             // 网络字节序端口
    unsigned long host_addr = 0x12345678; // 主机字节序地址
    unsigned long net_addr;              // 网络字节序地址

    // 将主机字节序转换为网络字节序
    net_port = htons(host_port);
    net_addr = htonl(host_addr);

    // 观察转换前后的结果
    printf("Host Byte Order Port: %#x\n", host_port);
    printf("NewWork Byte Order Port: %#x\n", net_port);
    printf("Host Byte Order Address: %#lx\n", host_addr);
    printf("NewWork Byte Order Address: %#lx\n", net_addr);

    return 0;
}
