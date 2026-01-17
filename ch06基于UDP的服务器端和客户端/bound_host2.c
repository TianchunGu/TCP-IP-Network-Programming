#include <arpa/inet.h>  // inet_addr, htons, sockaddr_in：IPv4 地址转换、字节序转换、地址结构
#include <stdio.h>       // printf, fputs：标准 I/O
#include <stdlib.h>      // exit, atoi：退出、字符串转整数
#include <string.h>      // memset：内存清零
#include <sys/socket.h>  // socket, sendto：套接字创建与 UDP 发送
#include <unistd.h>      // close：关闭文件描述符

#define BUF_SIZE 30
// ⚠️ 提示：BUF_SIZE 在本文件里并未使用（可能是从接收端示例复制来的宏）

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
  //
  // ⚠️ 隐患1：inet_addr 对非法 IP 的处理不够友好（失败返回
  // INADDR_NONE=0xFFFFFFFF，
  //   但该值也可能是合法广播地址 255.255.255.255，存在歧义）
  //   更现代/安全的做法是 inet_pton，但这里按要求不改代码，只提示风险。
  //
  // ⚠️ 隐患2：atoi 不做合法性检查，非数字会返回 0；端口 0 表示由系统分配端口（对
  // sendto 目标端口来说则是“发往 0 端口”，通常无意义）
  // -------------------------
  memset(&your_addr, 0, sizeof(your_addr));
  your_addr.sin_family = AF_INET;
  your_addr.sin_addr.s_addr = inet_addr(argv[1]);
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
  // ⚠️ 隐患3（很常见）：这里用 sizeof(msg1) 会把结尾 '\0' 一并发送出去。
  //   - 如果接收端按“字符串”打印，这通常没问题（甚至更安全，因为带 '\0'）
  //   - 但如果协议约定不需要 '\0'，这就会多发 1 字节，可能导致协议不一致
  //   - 对 msg2、msg3 同理
  //
  // ⚠️ 隐患4：没有检查 sendto 返回值。
  //   sendto 返回实际发送的字节数，失败返回
  //   -1；不检查会导致“发送失败但程序仍正常退出”。
  //
  // ⚠️ 隐患5：UDP 不可靠、不保证到达、不保证顺序、也不保证不重复。
  //   连续发三次并不代表对端一定按 1/2/3 的顺序收到，也不保证一定收到三条。
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
