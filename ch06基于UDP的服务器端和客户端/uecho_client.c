#include <arpa/inet.h>  // inet_addr, htons, sockaddr_in：IP 转换、字节序转换、IPv4 地址结构
#include <stdio.h>       // printf, fputs, fgets：标准输入输出与行输入
#include <stdlib.h>      // exit, atoi：退出程序、字符串转整数（端口号）
#include <string.h>      // memset, strcmp, strlen：清零、字符串比较、字符串长度
#include <sys/socket.h>  // socket, sendto, recvfrom：UDP 套接字 API
#include <unistd.h>      // close：关闭文件描述符

#define BUF_SIZE \
  30  // 输入/接收缓冲区大小：最多处理 29 个字符 + 可能的 '\0'（视情况）
void error_handling(char* message);  // 错误处理函数：打印错误并退出

int main(int argc, char* argv[]) {
  int sock;  // UDP socket 描述符（发送/接收数据报）
  char message
      [BUF_SIZE];  // 发送/接收缓冲区：既用于发送用户输入，也用于接收服务器回显
  int str_len;     // recvfrom 返回的接收字节数（失败为 -1）

  struct sockaddr_in serv_addr,
      clnt_addr;  // serv_addr：服务器地址（目标）；clnt_addr：收到回复的对端地址
  socklen_t clnt_addr_sz;  // 地址结构体长度：传入 recvfrom，返回实际长度

  // -------------------------
  // 参数检查：必须提供服务器 IP 与端口
  // argv[1] = <IP>，argv[2] = <port>
  // -------------------------
  if (argc != 3) {
    printf("Usage: %s <IP> <port>\n", argv[0]);
    exit(1);
  }

  // -------------------------
  // 创建 UDP socket
  // PF_INET：IPv4
  // SOCK_DGRAM：UDP 数据报
  // protocol=0：系统选择默认 UDP 协议
  // -------------------------
  sock = socket(PF_INET, SOCK_DGRAM, 0);
  if (sock == -1)
    error_handling("socket() error");

  // -------------------------
  // 初始化服务器地址结构（目的地址）
  // memset 清零：避免未初始化字段干扰 sendto/recvfrom
  // sin_family：IPv4
  // sin_addr.s_addr：将字符串 IP 转为网络序的 32 位地址
  // sin_port：端口号转网络字节序
  //
  // ⚠️ 隐患1：inet_addr 对非法 IP 的失败返回值是 INADDR_NONE(0xFFFFFFFF)，
  //   但 255.255.255.255 也会得到同样的值，存在歧义；更推荐 inet_pton。
  // ⚠️ 隐患2：atoi 不做合法性检查，非数字会变成 0；端口 0 通常不符合预期。
  // -------------------------
  memset(&serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = inet_addr(argv[1]);
  serv_addr.sin_port = htons(atoi(argv[2]));

  // -------------------------
  // 主循环：从标准输入读取一行 -> 发给服务器 -> 等待服务器回显 -> 打印
  // ⚠️ UDP 无连接：这里并没有 connect()，每次 sendto 指定目的地址即可。
  // -------------------------
  while (1) {
    // 提示用户输入消息；约定输入 q 或 Q 退出
    fputs("Insert message: (q/Q to quit): ", stdout);

    // 从 stdin 读取一行，最多读 BUF_SIZE-1 个字符，并自动补 '\0'
    // ⚠️ 隐患3：如果用户输入长度 >= BUF_SIZE，fgets 会截断，只读到缓冲区满为止；
    //   剩余内容会留在输入缓冲区，可能影响下一次读取。
    // ⚠️ 隐患4：fgets 失败（如 EOF）会返回 NULL，这里未检查，后续 strcmp/strlen
    // 可能崩溃。
    fgets(message, BUF_SIZE, stdin);

    // 如果用户输入 "q\n" 或 "Q\n" 则退出循环
    // strcmp 返回 0 表示字符串相等
    if (!strcmp(message, "q\n") || !strcmp(message, "Q\n"))
      break;

    // -------------------------
    // sendto：发送 UDP 数据报到服务器
    // 发送长度使用 strlen(message)：不包含末尾 '\0'
    // 这样服务器收到的是“纯文本内容（含换行）”，不额外带 C 字符串结束符
    //
    // ⚠️ 隐患5：未检查 sendto 返回值（失败返回
    // -1），可能出现发送失败却继续等待接收导致阻塞。 ⚠️ 隐患6：UDP
    // 不保证可靠性/顺序/不丢包；发送后不一定能收到回显。
    // -------------------------
    sendto(sock, message, strlen(message), 0, (struct sockaddr*)&serv_addr,
           sizeof(serv_addr));

    // -------------------------
    // recvfrom：阻塞等待接收一个 UDP 数据报
    // 返回值 str_len 是实际收到的字节数（可能为 0，失败为 -1）
    // 同时 clnt_addr 会被填充为“发送该数据报的对端地址”
    // -------------------------
    clnt_addr_sz = sizeof(clnt_addr);
    str_len = recvfrom(sock, message, BUF_SIZE, 0, (struct sockaddr*)&clnt_addr,
                       &clnt_addr_sz);

    // -------------------------
    // 将接收到的数据当作字符串处理：在末尾补 '\0'
    //
    // ⚠️ 隐患7（非常关键）：如果 str_len == BUF_SIZE，
    //   那么 message[str_len] = 0 会写到 message[BUF_SIZE]
    //   位置，发生越界写（未定义行为）。 更安全的做法通常是 recvfrom 只读
    //   BUF_SIZE-1 字节并再补 '\0'， 但按你的要求这里不改代码，只在注释提醒。
    //
    // ⚠️ 隐患8：若 recvfrom 失败返回 -1，则 str_len 为 -1，
    //   message[str_len] = 0 会写到 message[-1]（严重越界），风险更大。
    //   因此实际工程中必须先判断 str_len > 0 再写终止符。
    // -------------------------
    message[str_len] = 0;

    // 打印服务器返回的消息
    // 这里假设服务器回的就是可打印文本，并且已经在上面补了 '\0'
    printf("Message from server: %s\n", message);

    // ⚠️ 隐患9（协议/安全层面）：UDP 无连接，recvfrom
    // 收到的包不一定来自“真正的服务器”；
    //   当前代码没有校验 clnt_addr 是否等于
    //   serv_addr，存在被第三方伪造/注入回复的风险。
    //   （例如：有人向你的临时端口发包，你也会当成“服务器回复”打印出来）
  }

  // 关闭 socket
  close(sock);

  return 0;
}

void error_handling(char* message) {
  // 输出错误信息到标准错误并退出
  // ⚠️ 这里未输出 errno 细节（如 perror），定位问题时信息可能不够
  fputs(message, stderr);
  fputc('\n', stderr);
  exit(1);
}
