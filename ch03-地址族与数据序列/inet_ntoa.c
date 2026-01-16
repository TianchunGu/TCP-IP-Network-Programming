#include <stdio.h>     // printf
#include <string.h>    // strcpy
#include <arpa/inet.h> // inet_ntoa, htonl, sockaddr_in

int main(int argc, char* argv[])
{
    struct sockaddr_in addr1, addr2; // IPv4 地址结构体
    char *str_ptr;                   // 指向 inet_ntoa 返回的静态字符串
    char str_arr[20];                // 手动拷贝保存的字符串缓冲区

    // 构造两个网络字节序的 IPv4 地址
    addr1.sin_addr.s_addr = htonl(0x1020304);
    addr2.sin_addr.s_addr = htonl(0x1010101);

    // inet_ntoa 返回静态缓冲区指针，后续调用会覆盖该内容
    str_ptr = inet_ntoa(addr1.sin_addr);
    // 拷贝一份，防止后续调用覆盖
    strcpy(str_arr, str_ptr);
    printf("Dotted-Decimal notation1: %s\n", str_ptr);

    // 再次调用 inet_ntoa，会覆盖之前的静态缓冲区
    inet_ntoa(addr2.sin_addr);
    printf("Dotted-Decimal notation2: %s\n", str_ptr);
    // 输出之前拷贝的字符串，仍保持第一次的结果
    printf("Dotted-Decimal notation3: %s\n", str_arr);

    return 0;
}

// output:
// lxc@Lxc:~/C/tcpip_src/ch03-地址族与数据序列$ bin/inet_ntoa 
// Dotted-Decimal notation1: 1.2.3.4
// Dotted-Decimal notation2: 1.1.1.1
// Dotted-Decimal notation3: 1.2.3.4
