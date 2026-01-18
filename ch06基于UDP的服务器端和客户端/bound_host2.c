#include <arpa/inet.h>  // inet_addr, htons, sockaddr_in：IPv4 地址转换、字节序转换、地址结构
#include <stdio.h>       // printf, fputs：标准 I/O
#include <stdlib.h>      // exit, atoi：退出、字符串转整数
#include <string.h>      // memset：内存清零
#include <sys/socket.h>  // socket, sendto：套接字创建与 UDP 发送
#include <unistd.h>      // close：关闭文件描述符

#define BUF_SIZE 30

void error_handling(char* message);  // 错误处理函数声明：输出错误并退出

int main(int argc, char* argv[]) {
  int sock;  // UDP socket 文件描述符

  // 三条要发送的消息
  // 注意：这里是“字符数组”，而不是 char* 指针常量
  // 因为是数组，所以 sizeof(msgX) 会得到数组总大小（包含结尾 '\0'）
  char msg1[] = "Hi!";
  char msg2[] = "I'm another UDP host!";
  char msg3[] = "Nice to meet you";

  // 保存对端（接收方）地址：IP + port
  struct sockaddr_in your_addr;

  // -------------------------
  // 参数检查：要求提供 <IP> 和 <port>
  // argv[1]：目标 IP（点分十进制字符串）
  // argv[2]：目标端口号字符串
  // -------------------------
  if (argc != 3) {
    printf("Usage: %s <IP> <port>\n", argv[0]);
    exit(1);
  }

  // -------------------------
  // 创建 UDP socket
  // PF_INET：IPv4
  // SOCK_DGRAM：UDP（数据报）
  // protocol=0：由系统选择默认协议（UDP）
  // -------------------------
  sock = socket(PF_INET, SOCK_DGRAM, 0);
  if (sock == -1)
    error_handling("socket() error");

  // -------------------------
  // 初始化对端地址结构
  // memset 清零：避免未初始化字段影响系统调用
  // sin_family：AF_INET 表示 IPv4
  // sin_addr.s_addr：把字符串 IP 转为网络序的 32 位地址
  // sin_port：端口号转网络字节序（16 位）
  // -------------------------
  memset(&your_addr, 0, sizeof(your_addr));
  your_addr.sin_family = AF_INET;
  //inet_addr 函数不仅能完成字符串到整数的转换，还会自动将结果转换为网络字节序。
  your_addr.sin_addr.s_addr = inet_addr(argv[1]);
  // 端口号通过atoi转换为整数，再由htons转换为网络字节序
  your_addr.sin_port = htons(atoi(argv[2]));

  // -------------------------
  // sendto：向指定地址发送一个 UDP 数据报
  // 参数说明：
  // sock：发送用 UDP socket
  // msg1：要发送的数据缓冲区
  // sizeof(msg1)：发送长度（字节数）
  // flags=0：默认
  // (struct sockaddr*)&your_addr：目标地址
  // sizeof(your_addr)：目标地址结构体长度
  //
  // -------------------------
  sendto(sock, msg1, sizeof(msg1), 0, (struct sockaddr*)&your_addr,
         sizeof(your_addr));

  sendto(sock, msg2, sizeof(msg2), 0, (struct sockaddr*)&your_addr,
         sizeof(your_addr));

  sendto(sock, msg3, sizeof(msg3), 0, (struct sockaddr*)&your_addr,
         sizeof(your_addr));

  // 关闭 socket，释放系统资源
  close(sock);

  return 0;
}

void error_handling(char* message) {
  // 将错误信息输出到标准错误（stderr），并换行
  // 然后直接终止进程
  fputs(message, stderr);
  fputc('\n', stderr);
  exit(1);
}
