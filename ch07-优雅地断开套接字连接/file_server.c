#include <arpa/inet.h>  // htonl, htons, sockaddr_in：字节序转换、IPv4 地址结构
#include <stdio.h>  // printf, fputs, FILE, fopen, fread, fclose：标准 I/O 与文件读写
#include <stdlib.h>      // exit, atoi：退出程序、字符串转整数（端口号）
#include <string.h>      // memset：内存清零
#include <sys/socket.h>  // socket, bind, listen, accept, shutdown：套接字 API
#include <unistd.h>      // read, write, close：POSIX I/O（socket 读写、关闭）

#define BUF_SIZE 30                  // 每次从文件读/向网络写的块大小（字节）
void error_handling(char* message);  // 统一错误处理函数：打印并退出

int main(int argc, char* argv[]) {
  int serv_sock, clnt_sock;  // serv_sock：监听
                             // socket；clnt_sock：与客户端建立连接后的 socket
  char buf [BUF_SIZE];  // 缓冲区：用于从文件读取数据，再发送到网络；也用于接收客户端确认信息
  int read_cnt;  // fread/read 的返回值：读到的字节数
  FILE* fp;      // 文件指针：要发送的文件

  struct sockaddr_in serv_addr, clnt_addr;  // serv_addr：服务器绑定地址；clnt_addr：客户端地址（accept填充）
  socklen_t clnt_addr_sz;  // 客户端地址结构体长度（accept 需要）

  // -------------------------
  // 参数检查：期望提供 <port>
  // -------------------------
  if (argc != 2) {
    printf("Usage: %s <port>\n", argv[0]);
    exit(1);
  }

  // -------------------------
  // 打开要发送的文件（以二进制方式读取）
  // 这里固定发送 "file_server.c"
  // -------------------------
  fp = fopen("file_server.c", "rb");

  // -------------------------
  // 创建 TCP 监听 socket
  // PF_INET：IPv4
  // SOCK_STREAM：TCP
  // -------------------------
  serv_sock = socket(PF_INET, SOCK_STREAM, 0);
  if (serv_sock == -1)
    error_handling("socket() error");

  // -------------------------
  // 初始化服务器绑定地址
  // INADDR_ANY：绑定本机所有网卡
  // 端口从 argv[1] 解析并转网络字节序
  // -------------------------
  memset(&serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  serv_addr.sin_port = htons(atoi(argv[1]));

  // -------------------------
  // bind：绑定 IP:port
  // listen：开始监听，backlog=5（等待队列长度）
  // -------------------------
  if (bind(serv_sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == -1)
    error_handling("bind() error");

  if (listen(serv_sock, 5) == -1)
    error_handling("listen() error");

  // -------------------------
  // accept：阻塞等待一个客户端连接
  // 成功返回与该客户端通信的 clnt_sock
  // -------------------------
  clnt_addr_sz = sizeof(clnt_addr);
  clnt_sock = accept(serv_sock, (struct sockaddr*)&clnt_addr, &clnt_addr_sz);
  if (clnt_sock == -1)
    error_handling("accept() error");

  // -------------------------
  // 循环从文件读取 BUF_SIZE 字节并发送给客户端
  // fread(buf, 1, BUF_SIZE, fp)：
  //   - 尝试最多读取 BUF_SIZE 字节
  //   - 返回实际读取字节数：可能是 BUF_SIZE（读满），也可能更小（EOF 或错误）
  // -------------------------
  while (1) {
    read_cnt = fread((void*)buf, 1, BUF_SIZE, fp);

    // -------------------------
    // 如果这次读到的字节数 < BUF_SIZE，就认为到达文件末尾（或发生错误）：
    //   - 先把 read_cnt 字节发出去
    //   - 再 break 结束发送
    // -------------------------
    if (read_cnt < BUF_SIZE) {
      write(clnt_sock, buf, read_cnt);
      break;
    }

    // 正常情况：读满 BUF_SIZE 字节，发送 BUF_SIZE 字节
    write(clnt_sock, buf, BUF_SIZE);
  }

  // -------------------------
  // shutdown(clnt_sock, SHUT_WR)：
  // “半关闭”连接的写方向：表示服务器已发送完毕，不再发送数据
  // 但仍保持读方向打开，以便接收客户端发来的确认信息
  //
  // 这是常见技巧：客户端读到 EOF（read 返回 0）就知道文件结束了
  // -------------------------
  shutdown(clnt_sock, SHUT_WR);

  // -------------------------
  // 读取客户端的反馈消息（例如 "Thank you"）
  // -------------------------
  read(clnt_sock, buf, BUF_SIZE);
  printf("Message from client: %s\n", buf);

  // -------------------------
  // 资源释放：关闭文件与 socket
  // -------------------------
  fclose(fp);
  close(clnt_sock);
  close(serv_sock);

  return 0;
}

void error_handling(char* message) {
  // 输出错误信息并退出
  fputs(message, stderr);
  fputc('\n', stderr);
  exit(1);
}
