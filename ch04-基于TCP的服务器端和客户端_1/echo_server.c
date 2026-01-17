#include <arpa/inet.h>   // htonl, htons, sockaddr_in
#include <stdio.h>       // printf, fputs, stdout
#include <stdlib.h>      // exit, atoi
#include <string.h>      // memset
#include <sys/socket.h>  // socket, bind, listen, accept
#include <unistd.h>      // read, write, close

#define BUF_SIZE 1024  // 收发缓冲区大小
void error_handling(char* message);

int main(int argc, char* argv[]) {
  int serv_sock, clnt_sock;  // 监听套接字与客户端连接套接字
  char message[BUF_SIZE];    // 收发缓冲区
  int str_len;               // 实际读取的字节数

  struct sockaddr_in serv_addr, clnt_addr;  // 服务器与客户端地址
  socklen_t clnt_addr_sz;                   // 客户端地址结构体长度

  // 参数校验：需要端口号
  if (argc != 2) {
    printf("Usage: %s\n", argv[0]);
    exit(1);
  }

  // 创建 TCP 监听套接字
  if ((serv_sock = socket(PF_INET, SOCK_STREAM, 0)) == -1)
    error_handling("socket() error");

  // 初始化服务器地址并绑定
  memset(&serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  // 绑定到本机任意地址
  serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  // 端口号转换为网络字节序
  serv_addr.sin_port = htons(atoi(argv[1]));

  // 绑定套接字到指定地址
  if (bind(serv_sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == -1)
    error_handling("bind() error");

  // 开始监听
  if (listen(serv_sock, 5) == -1)
    error_handling("listen() error");

  // 依次处理最多 5 个客户端连接
  clnt_addr_sz = sizeof(clnt_addr);
  for (int i = 0; i < 5; i++) {
    // 阻塞等待客户端连接
    if ((clnt_sock = accept(serv_sock, (struct sockaddr*)&clnt_addr,
                            &clnt_addr_sz)) == -1)
      error_handling("accept() error");
    else
      printf("connected client: %d\n", i + 1);

    // 回显：读多少写多少，直到客户端关闭连接
    while ((str_len = read(clnt_sock, message, BUF_SIZE)) != 0) {
      // 将读到的数据当作字符串输出到服务器端终端
      message[str_len] = 0;
      fputs(message, stdout);
      // 原样回写给客户端
      write(clnt_sock, message, str_len);
    }

    // 单个客户端处理完毕，关闭连接
    close(clnt_sock);
  }

  // 关闭监听套接字
  close(serv_sock);

  return 0;
}

void error_handling(char* message) {
  // 将错误信息输出到标准错误并终止程序
  fputs(message, stderr);
  fputc('\n', stderr);
  exit(1);
}
