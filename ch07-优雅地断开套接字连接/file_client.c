#include <arpa/inet.h>  // inet_addr, htons, sockaddr_in：IP 转换、字节序转换、IPv4 地址结构
#include <stdio.h>  // printf, puts, fputs, FILE, fopen, fwrite, fclose：标准 I/O 与文件操作
#include <stdlib.h>      // exit, atoi：退出、字符串转整数（端口）
#include <string.h>      // memset：内存清零
#include <sys/socket.h>  // socket, connect：套接字创建、连接
#include <unistd.h>      // read, write, close：POSIX I/O（socket 读写、关闭）

#define BUF_SIZE 30                  // 每次从 socket 读取的缓冲区大小（字节）
void error_handling(char* message);  // 错误处理函数声明

int main(int argc, char* argv[]) {
  int serv_sock;       // 这里名字叫 serv_sock，但实际是“客户端
                       // socket”（连接到服务器用）
  char buf[BUF_SIZE];  // 接收缓冲区：保存从网络读到的一段二进制数据
  FILE* fp;            // 文件指针：用于把接收的数据写入本地文件
  int read_cnt;  // read() 的返回值：本次实际读取到的字节数（0 表示对端关闭，-1
                 // 表示出错）

  struct sockaddr_in serv_addr;  // 服务器地址结构体（目标 IP:port）

  // -------------------------
  // 参数检查：期望传入 <IP> <port>
  // -------------------------
  if (argc != 3) {
    printf("Usage: %s\n", argv[0]);
    exit(1);
  }

  // -------------------------
  // 创建 TCP socket
  // PF_INET：IPv4
  // SOCK_STREAM：面向连接字节流（TCP）
  // -------------------------
  serv_sock = socket(PF_INET, SOCK_STREAM, 0);
  if (serv_sock == -1)
    error_handling("socket() error");

  // -------------------------
  // 打开本地文件，准备以二进制方式写入接收内容
  // "wb"：write binary（覆盖写）
  // -------------------------
  fp = fopen("received.data", "wb");

  // -------------------------
  // 初始化服务器地址
  // memset 清零避免未初始化字段干扰 connect
  // sin_family：IPv4
  // sin_addr.s_addr：将字符串 IP 转成网络序地址
  // sin_port：端口转网络字节序
  // -------------------------
  memset(&serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = inet_addr(argv[1]);
  serv_addr.sin_port = htons(atoi(argv[2]));

  // -------------------------
  // connect：连接到服务器
  // -------------------------
  connect(serv_sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr));

  // -------------------------
  // 循环从 socket 读取数据并写入文件
  // 对 TCP 来说：
  //   - read 可能读到任意长度（1..BUF_SIZE），不保证“按包”对齐
  //   - read 返回 0 表示对端关闭连接（EOF），通常意味着文件发送完毕
  // -------------------------
  while ((read_cnt = read(serv_sock, buf, BUF_SIZE)) != 0)
    fwrite((void*)buf, 1, read_cnt, fp);

  // -------------------------
  // 文件接收完毕（对端关闭连接导致 read 返回 0），打印提示
  // -------------------------
  puts("Received file date");

  // -------------------------
  // 向服务器发送确认消息
  // write 发送 10 字节："Thank you" 长度 9，加上末尾 '\0' 1 字节 = 10
  // -------------------------
  write(serv_sock, "Thank you", 10);

  // 关闭文件与 socket，释放资源
  fclose(fp);
  close(serv_sock);

  return 0;
}

void error_handling(char* message) {
  // 输出错误信息并退出程序
  fputs(message, stderr);
  fputc('\n', stderr);
  exit(1);
}
