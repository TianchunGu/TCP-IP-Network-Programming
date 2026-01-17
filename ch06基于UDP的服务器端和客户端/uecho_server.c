#include <arpa/inet.h>  // htonl, htons, sockaddr_in：字节序转换、IPv4 地址结构
#include <stdio.h>      // printf, fputs：标准 I/O
#include <stdlib.h>     // exit, atoi：退出程序、字符串转整数（端口）
#include <string.h>     // memset：内存清零
#include <sys/socket.h>  // socket, bind, recvfrom, sendto：UDP 套接字相关系统调用
#include <unistd.h>      // close：关闭文件描述符

#define BUF_SIZE 30  // 接收缓冲区大小：一次最多接收 30 字节（可能会截断）
void error_handling(char* message);  // 错误处理函数：输出错误并退出

int main(int argc, char* argv[]) {
  // 服务器 UDP socket（用于接收/发送数据报）
  int serv_sock;
  // 接收缓冲区：存放收到的 UDP 数据报内容（原始字节流）
  char message[BUF_SIZE];
  // recvfrom 返回的“实际接收字节数”（失败为 -1）
  int str_len;

  // serv_addr：本机绑定地址；clnt_addr：客户端地址（recvfrom填充）
  struct sockaddr_in serv_addr, clnt_addr;
  // 客户端地址结构体大小（recvfrom 需要输入/输出）
  socklen_t clnt_addr_sz;

  // -------------------------
  // 参数检查：必须提供监听端口号
  // -------------------------
  if (argc != 2) {
    printf("Usage: %s <port>\n", argv[0]);
    exit(1);
  }

  // -------------------------
  // 创建 UDP socket
  // PF_INET：IPv4
  // SOCK_DGRAM：UDP 数据报（无连接、保留消息边界）
  // protocol=0：让系统选择默认协议（UDP）
  // -------------------------
  serv_sock = socket(PF_INET, SOCK_DGRAM, 0);
  if (serv_sock == -1)
    error_handling("socket() error");

  // -------------------------
  // 初始化服务器地址并准备绑定
  // memset 清零：避免结构体中未初始化字段影响 bind
  // sin_family：IPv4
  // sin_addr.s_addr：INADDR_ANY
  // 表示绑定本机任意网卡地址（0.0.0.0），需要转网络序 sin_port：绑定端口（从
  // argv[1] 解析），需要转网络序
  //
  // ⚠️ 隐患1：atoi 不检查合法性，非数字会得到 0；端口 0
  // 表示系统随机分配端口（通常不是想要的） ⚠️
  // 隐患2：未检查端口范围（1~65535），溢出/负值等都可能导致异常行为
  // -------------------------
  memset(&serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  serv_addr.sin_port = htons(atoi(argv[1]));

  // -------------------------
  // bind：将 UDP socket 绑定到本机端口
  // 绑定后才能在该端口上接收数据报
  // -------------------------
  if (bind(serv_sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == -1)
    error_handling("bind() error");

  // -------------------------
  // 主循环：不断接收客户端发来的数据报，并把收到的数据原样回发（echo）
  //
  // ⚠️ 隐患3：while(1) 死循环没有退出条件，close(serv_sock) 实际上永远执行不到
  //   - 真实服务一般需要信号处理（SIGINT/SIGTERM）或设置运行条件
  // -------------------------
  while (1) {
    // 每次接收前都要把 clnt_addr_sz 设置为结构体大小
    // recvfrom 会把发送方地址写入 clnt_addr，并更新 clnt_addr_sz
    clnt_addr_sz = sizeof(clnt_addr);

    // -------------------------
    // recvfrom：阻塞等待接收 UDP 数据报
    // 参数：
    // serv_sock：监听 socket
    // message：接收缓冲区
    // BUF_SIZE：最多接收 BUF_SIZE 字节
    // flags=0：默认阻塞
    // &clnt_addr：返回发送方地址
    // &clnt_addr_sz：输入为 clnt_addr 大小，输出为实际地址长度
    //
    // ⚠️ 隐患4：未检查返回值 str_len
    //   - str_len == -1：接收失败（可能被信号打断 EINTR，或其他错误）
    //   - str_len == 0：收到 0 长度数据报（也是可能的）
    //
    // ⚠️ 隐患5：如果客户端发送的数据报长度 > BUF_SIZE，会被截断，只保留前
    // BUF_SIZE 字节；
    //   UDP 的“多出来部分”会被丢弃，无法在后续 recvfrom 中继续读取（与 TCP
    //   不同）。
    //
    // ⚠️ 隐患6：message 是“原始字节缓冲区”，并不保证包含 '\0' 结尾；
    //   虽然这里没有用 %s 打印，但如果后续改成字符串处理，必须注意补 '\0'
    //   或按长度处理。
    // -------------------------
    str_len = recvfrom(serv_sock, message, BUF_SIZE, 0,
                       (struct sockaddr*)&clnt_addr, &clnt_addr_sz);

    // -------------------------
    // sendto：把收到的数据原样回发给发送方（回显）
    // 发送长度使用 str_len，确保“回发与收到长度一致”
    //
    // ⚠️ 隐患7：同样未检查 sendto 返回值：
    //   - 失败返回 -1
    //   - UDP
    //   虽然通常“要么发出去要么失败”，但仍建议检查以发现错误（如网络不可达等）
    //
    // ⚠️ 隐患8：UDP 不保证可靠性/顺序/去重：
    //   - 客户端不一定收到回显
    //   - 可能乱序或重复
    // -------------------------
    sendto(serv_sock, message, str_len, 0, (struct sockaddr*)&clnt_addr,
           clnt_addr_sz);
  }

  // ⚠️ 注意：由于 while(1) 没有 break，这里理论上不会执行到
  close(serv_sock);

  return 0;
}

void error_handling(char* message) {
  // 输出错误信息到标准错误并退出
  // ⚠️ 这里没有输出 errno 细节（例如 perror），调试时信息可能不够
  fputs(message, stderr);
  fputc('\n', stderr);
  exit(1);
}
