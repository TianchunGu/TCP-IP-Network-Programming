#include <arpa/inet.h>   // htonl, htons, sockaddr_in：字节序转换、IPv4 地址结构
#include <stdio.h>       // printf, fputs：标准输入输出
#include <stdlib.h>      // exit, atoi：退出、字符串转整数（端口）
#include <string.h>      // memset：内存清零
#include <sys/socket.h>  // socket, bind, recvfrom：套接字相关系统调用
#include <unistd.h>      // sleep, close：休眠、关闭描述符

#define BUF_SIZE 30  // 接收缓冲区大小：最多接收 30 字节（注意可能不含 '\0'）
void error_handling(char* message);  // 统一错误处理函数声明

int main(int argc, char* argv[]) {
  int sock;                // UDP 套接字描述符（文件描述符）
  char message[BUF_SIZE];  // 接收缓冲区：存放从网络收到的字节流
  struct sockaddr_in my_addr, your_addr;  // my_addr：本机地址（绑定用）；your_addr：对端地址（recvfrom
                                          // 填充）
  socklen_t addr_sz;  // 地址结构体长度（传入 recvfrom，返回实际长度）
  int str_len;        // recvfrom 返回值：本次接收的字节数（或 -1 表示失败）

  // -------------------------
  // 参数检查：要求命令行提供端口号
  // ⚠️ 隐患：Usage 提示里没有打印参数格式（应含
  // <port>），但这里按“不改代码”只提示风险
  // -------------------------
  if (argc != 2) {
    printf("Usage: %s\n", argv[0]);  // 实际应提示端口参数，但此处原代码如此
    exit(1);
  }

  // -------------------------
  // 创建 UDP socket
  // PF_INET：IPv4
  // SOCK_DGRAM：数据报协议（UDP），无连接、保留消息边界
  // protocol=0：由系统根据类型自动选择（UDP）
  // -------------------------
  sock = socket(PF_INET, SOCK_DGRAM, 0);
  if (sock == -1)
    error_handling("socket() error");

  // -------------------------
  // 初始化并设置本机绑定地址
  // memset 清零：避免结构体中未初始化字段导致异常
  // sin_family：IPv4
  // sin_addr.s_addr：绑定任意网卡地址（INADDR_ANY），需要转成网络字节序
  // sin_port：绑定端口（argv[1]），需要转成网络字节序
  //
  // ⚠️ 隐患1：atoi 不做错误检查，非数字/溢出会导致端口异常（例如返回 0）
  // ⚠️ 隐患2：端口为 0 表示由系统分配随机端口，可能不是你想要的
  // -------------------------
  memset(&my_addr, 0, sizeof(my_addr));
  my_addr.sin_family = AF_INET;
  my_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  my_addr.sin_port = htons(atoi(argv[1]));

  // -------------------------
  // bind：把 UDP socket 绑定到本机 IP:port
  // 绑定后才能在该端口上接收 datagram
  // -------------------------
  if (bind(sock, (struct sockaddr*)&my_addr, sizeof(my_addr)) == -1)
    error_handling("bind() error");

  // -------------------------
  // 接收 3 次 UDP 数据报并打印
  // UDP 特点：
  // 1) 无连接：谁发来的包都能收，recvfrom 会填充发送方地址到 your_addr
  // 2) 保留消息边界：一次 recvfrom 对应接收一个 datagram（可能被截断）
  // -------------------------
  for (int i = 0; i < 3; i++) {
    // 每次接收前先 sleep 5 秒
    // ⚠️ 隐患：人为延迟会降低响应性；如果想周期性处理，可考虑超时/事件驱动等
    sleep(5);

    // recvfrom 需要传入地址结构体长度
    addr_sz = sizeof(your_addr);

    // -------------------------
    // recvfrom：阻塞等待接收 UDP 数据报
    // 参数说明：
    // sock：绑定好的 UDP socket
    // message：接收缓冲区
    // BUF_SIZE：最多拷贝 BUF_SIZE 字节到 message
    // flags=0：默认阻塞接收
    // (struct sockaddr*)&your_addr：返回发送方地址
    // &addr_sz：输入为结构体大小，输出为实际地址长度
    //
    // ⚠️ 隐患1（非常关键）：recvfrom 返回的字节流不保证以 '\0' 结尾。
    //   但下面 printf 用 %s 当 C 字符串打印，可能：
    //   - 打印越界（一直打印到内存中偶然出现 '\0'）
    //   - 泄露栈上数据/输出乱码
    //   - 触发未定义行为
    //
    // ⚠️ 隐患2：如果发送的 datagram 长度 > BUF_SIZE，会被截断，只保留前 BUF_SIZE
    // 字节；
    //   但 str_len 仍返回“实际拷贝到缓冲区的字节数”，无法恢复剩余内容。
    //
    // ⚠️ 隐患3：未检查 str_len 是否为 -1（出错）；
    //   也未检查 str_len==0 的情况（UDP 一般也可能收到 0 长度数据报）。
    // -------------------------
    str_len = recvfrom(sock, message, BUF_SIZE, 0, (struct sockaddr*)&your_addr,
                       &addr_sz);

    // -------------------------
    // 打印收到的第 i+1 条消息
    // ⚠️ 隐患：如上所述，message 可能不是字符串，使用 %s 存在越界风险
    // ⚠️ 另外：即使发送方发送了 '\0'，如果刚好填满 BUF_SIZE，也可能无终止符
    // -------------------------
    printf("Message %d:%s \n", i + 1, message);
  }

  // 关闭 socket，释放系统资源
  close(sock);

  return 0;
}

void error_handling(char* message) {
  // 将错误信息输出到标准错误（stderr），并换行
  // 然后终止进程
  fputs(message, stderr);
  fputc('\n', stderr);
  exit(1);
}
