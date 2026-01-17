#include <arpa/inet.h>   // htonl, htons, sockaddr_in：字节序转换、IPv4 地址结构
#include <stdio.h>       // printf, fputs：标准 I/O（输出提示、错误信息）
#include <stdlib.h>      // exit, atoi：退出程序、字符串转整数（端口号）
#include <string.h>      // memset：清零结构体/缓冲区
#include <sys/socket.h>  // socket, bind, listen, accept：套接字 API
#include <unistd.h>  // read, write, close：POSIX I/O（读写 socket、关闭描述符）

#define BUF_SIZE 1024  // 接收缓冲区大小（用于暂存客户端发送的数据）
#define OPSZ \
  4  // 每个操作数占用字节数：这里假设 int 为 4 字节（常见但并非标准保证）

// 统一错误处理：打印错误并退出
void error_handling(char* message);

// 计算函数：根据 operator 对 opnds 中的 opnum 个操作数做运算
int calculate(int opnum, int opnds[], char operator);

int main(int argc, char* argv[]) {
  int serv_sock, clnt_sock;  // serv_sock：监听
                             // socket；clnt_sock：与某个客户端已连接的 socket
  char opinfo[BUF_SIZE];  // 用于接收“操作数数组 + 运算符”的缓冲区（按协议布局）
  int result, opnd_cnt,
      i;  // result：计算结果；opnd_cnt：操作数个数；i：循环计数
  int recv_cnt, recv_len;  // recv_cnt：本次 read
                           // 实际读取字节数；recv_len：累计已读取字节数
  struct sockaddr_in serv_addr,
      clnt_addr;  // serv_addr：服务器地址；clnt_addr：客户端地址（accept 填充）
  socklen_t clnt_addr_sz;  // 客户端地址结构体长度（传给 accept/返回）

  // 参数检查：程序启动格式必须为 ./server <port>
  if (argc != 2) {
    printf("Usage: %s <port>\n", argv[0]);
    exit(1);
  }

  // =========================
  // 1) 创建 TCP 监听套接字
  // PF_INET：IPv4；SOCK_STREAM：面向连接的字节流（TCP）
  // 返回值是文件描述符；-1 表示失败
  // =========================
  serv_sock = socket(PF_INET, SOCK_STREAM, 0);
  if (serv_sock == -1)
    error_handling("socket() error");

  // =========================
  // 2) 初始化服务器地址结构体并绑定端口
  // memset 清零：避免结构体中未初始化字段导致异常
  // sin_family：地址族 IPv4
  // sin_addr.s_addr：绑定到本机任意网卡（INADDR_ANY），注意要转为网络字节序
  // sin_port：监听端口（从 argv[1] 解析），注意要转为网络字节序
  // =========================
  memset(&serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr =
      htonl(INADDR_ANY);  // 主机字节序 -> 网络字节序（32 位）
  serv_addr.sin_port =
      htons(atoi(argv[1]));  // 主机字节序 -> 网络字节序（16 位）

  // =========================
  // 3) bind：将 socket 与“IP:port”绑定
  // 4) listen：进入监听状态，等待客户端连接
  // listen 的 5 表示等待队列(backlog)大小（不是“最多 5 个客户端”的严格限制）
  // =========================
  if (bind(serv_sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == -1)
    error_handling("bind() error");
  if (listen(serv_sock, 5) == -1)
    error_handling("listen() error");

  // clnt_addr_sz 用来告诉 accept：clnt_addr 结构体的大小
  clnt_addr_sz = sizeof(clnt_addr);

  // =========================
  // 5) 依次处理最多 5 个客户端（串行、阻塞式）
  // 每次循环：accept -> 收请求 -> 计算 -> 回写结果 -> close
  // =========================
  for (i = 0; i < 5; i++) {
    opnd_cnt = 0;

    // -------------------------
    // accept：阻塞等待客户端连接
    // 成功后返回一个新的连接 socket（clnt_sock），用于与该客户端通信
    // -------------------------
    clnt_sock = accept(serv_sock, (struct sockaddr*)&clnt_addr, &clnt_addr_sz);
    if (clnt_sock == -1)
      error_handling("accept() error");

    // -------------------------
    // 先读取 1 字节的“操作数个数”
    // 协议约定：客户端先发 1 字节 opnd_cnt，然后发 opnd_cnt 个 int（每个 4
    // 字节），最后发 1 字节运算符
    // -------------------------
    read(clnt_sock, &opnd_cnt, 1);

    recv_len = 0;

    // -------------------------
    // 再读取剩余数据：
    //   操作数：opnd_cnt * OPSZ（假设 int=4字节）
    //   运算符：1 字节
    // 因为 TCP 是“字节流”，一次 read 不保证读满，需要循环累计
    // -------------------------
    while (recv_len < (opnd_cnt * OPSZ + 1)) {
      // 从 clnt_sock 读取数据，放到 opinfo 的 recv_len 偏移处
      // BUF_SIZE - 1：这里保留 1
      recv_cnt = read(clnt_sock, &opinfo[recv_len], BUF_SIZE - 1);
      recv_len += recv_cnt;
    }

    // -------------------------
    // opinfo 的内存布局（按协议）：
    //   [0 ... opnd_cnt*4 - 1]       ：opnd_cnt 个操作数（原样二进制）
    //   [opnd_cnt*4]                 ：运算符（1 字节）
    //
    // (int*)opinfo：把缓冲区前面那段“按 int 数组解释”
    // opinfo[recv_len - 1]：最后 1 字节当作运算符
    // -------------------------
    result = calculate(opnd_cnt, (int*)opinfo, opinfo[recv_len - 1]);

    // -------------------------
    // 将 4 字节 result 回写给客户端
    // -------------------------
    write(clnt_sock, (char*)&result, sizeof(result));

    // 关闭与当前客户端的连接 socket（只关闭这个连接，不影响监听 socket）
    close(clnt_sock);
  }

  // 关闭监听 socket（服务器退出）
  close(serv_sock);

  return 0;
}

int calculate(int opnum, int opnds[], char op) {
  // 默认以第一个操作数为初始值
  int result = opnds[0];

  switch (op) {
    case '+':
      // 从第 2 个操作数开始依次相加
      for (int i = 1; i < opnum; i++)
        result += opnds[i];
      break;

    case '-':
      // 从第 2 个操作数开始依次相减：result = opnds[0] - opnds[1] - opnds[2] -
      // ...
      for (int i = 1; i < opnum; i++)
        result -= opnds[i];
      break;

    case '*':
      // 从第 2 个操作数开始依次相乘
      for (int i = 1; i < opnum; i++)
        result *= opnds[i];
      break;

    default:
      // 未知运算符：这里选择“不处理”，直接返回初始值
      // 更严格可以返回错误码或关闭连接
      break;
  }

  return result;
}

void error_handling(char* message) {
  // 将错误信息输出到标准错误（stderr），并追加换行
  // 然后立即终止整个进程
  fputs(message, stderr);
  fputc('\n', stderr);
  exit(1);
}
