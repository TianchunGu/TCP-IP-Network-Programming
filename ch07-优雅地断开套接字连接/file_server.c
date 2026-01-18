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
  char buf
      [BUF_SIZE];  // 缓冲区：用于从文件读取数据，再发送到网络；也用于接收客户端确认信息
  int read_cnt;  // fread/read 的返回值：读到的字节数
  FILE* fp;      // 文件指针：要发送的文件

  struct sockaddr_in serv_addr,
      clnt_addr;  // serv_addr：服务器绑定地址；clnt_addr：客户端地址（accept
                  // 填充）
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
  //
  // ⚠️ 隐患1：未检查 fopen 是否成功（可能返回 NULL：文件不存在/权限不足等）
  //   若 fp==NULL，后续 fread 会崩溃或产生未定义行为。
  //
  // ⚠️ 隐患2：文件名硬编码：
  //   - 只能发送固定文件
  //   - 可能泄露源代码/不符合期望
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
  //
  // ⚠️ 隐患3：atoi 不做合法性检查，非数字/越界可能导致端口异常（如 0）
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
  //
  // ⚠️ 隐患4：本程序只 accept 一次，只服务“一个客户端”，其余连接将排队/超时。
  // ⚠️ 隐患5：若 accept 被信号中断可能返回 -1（EINTR），这里不区分原因直接退出。
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
    //
    // ⚠️ 隐患6：read_cnt < BUF_SIZE 并不一定是“正常 EOF”，也可能是 fread 出错。
    //   正确区分需要 ferror(fp)/feof(fp)，但这里未检查。
    //
    // ⚠️ 隐患7：未检查 write 返回值：
    //   - TCP write 可能短写（只写入部分字节），需要循环写完
    //   - write 也可能返回 -1（例如连接断开），这里会忽略错误继续逻辑
    // -------------------------
    if (read_cnt < BUF_SIZE) {
      write(clnt_sock, buf, read_cnt);
      break;
    }

    // 正常情况：读满 BUF_SIZE 字节，发送 BUF_SIZE 字节
    // ⚠️ 同样存在短写/错误未处理问题（见上面隐患7）
    write(clnt_sock, buf, BUF_SIZE);
  }

  // -------------------------
  // shutdown(clnt_sock, SHUT_WR)：
  // “半关闭”连接的写方向：表示服务器已发送完毕，不再发送数据
  // 但仍保持读方向打开，以便接收客户端发来的确认信息
  //
  // 这是常见技巧：客户端读到 EOF（read 返回 0）就知道文件结束了
  //
  // ⚠️ 隐患8：未检查 shutdown 返回值（失败 -1）
  // -------------------------
  shutdown(clnt_sock, SHUT_WR);

  // -------------------------
  // 读取客户端的反馈消息（例如 "Thank you"）
  // ⚠️ 隐患9：未检查 read 返回值：
  //   - 可能返回 -1（出错）或 0（对端关闭）
  // ⚠️ 隐患10：读取到的数据不保证以 '\0' 结尾，
  //   但下面 printf 用 %s 当字符串打印，可能越界打印/乱码/泄露内存。
  //   （尤其当客户端发的是二进制或没有 '\0'）
  // ⚠️ 隐患11：只 read 一次，若客户端发的消息超过 BUF_SIZE
  // 或发生拆分，可能读不全。
  // -------------------------
  read(clnt_sock, buf, BUF_SIZE);
  printf("Message from client: %s\n", buf);

  // -------------------------
  // 资源释放：关闭文件与 socket
  // ⚠️ 若 fp 打开失败为 NULL，则 fclose(fp) 会出错（见隐患1）
  // -------------------------
  fclose(fp);
  close(clnt_sock);
  close(serv_sock);

  return 0;
}

void error_handling(char* message) {
  // 输出错误信息并退出
  // ⚠️ 未打印 errno 细节（perror），排错信息有限
  fputs(message, stderr);
  fputc('\n', stderr);
  exit(1);
}
