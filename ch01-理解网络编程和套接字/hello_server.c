#include <arpa/inet.h>   // htonl, htons, sockaddr_in
#include <stdio.h>       // 标准输入输出
#include <stdlib.h>      // exit, atoi
#include <string.h>      // memset
#include <sys/socket.h>  // socket, bind, listen, accept
#include <unistd.h>      // close, write

// 统一的错误处理：输出错误信息并终止程序
void error_handing(char* message);

int main(int argc, char* argv[]) {
  // 服务器端监听套接字与客户端连接套接字
  int serv_sock;
  int clnt_sock;

  // 服务器端与客户端地址结构体
  struct sockaddr_in serv_addr;
  struct sockaddr_in clnt_addr;
  socklen_t clnt_addr_size;

  // 发送给客户端的固定消息
  char message[] = "Hello World!";

  // 参数校验：必须传入端口号
  if (argc != 2) {
    printf("Usage: %s <port>\n", argv[0]);
    exit(1);
  }

  // 创建 TCP 套接字（IPv4, 面向连接）
  serv_sock = socket(PF_INET, SOCK_STREAM, 0);
  if (serv_sock == -1)
    error_handing("socket() error");

  // 初始化服务器地址结构体
  memset(&serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  // 绑定到任意本地地址
  serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  // 端口号转换为网络字节序
  serv_addr.sin_port = htons(atoi(argv[1]));

  // 绑定套接字到指定 IP/端口
  if (bind(serv_sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == -1)
    error_handing("bind() error");

  // 开始监听连接请求，等待队列长度为 5
  if (listen(serv_sock, 5) == -1)
    error_handing("listen() error");

  // 接受客户端连接（阻塞直到有连接到达）
  clnt_addr_size = sizeof(clnt_addr);
  clnt_sock = accept(serv_sock, (struct sockaddr*)&clnt_addr, &clnt_addr_size);
  if (clnt_sock == -1)
    error_handing("accept() error");

  // 向客户端发送消息，然后关闭连接
  write(clnt_sock, message, sizeof(message));
  close(clnt_sock);
  close(serv_sock);

  return 0;
}

void error_handing(char* message) {
  // 将错误信息输出到标准错误并退出
  fputs(message, stderr);
  fputc('\n', stderr);
  exit(1);
}
