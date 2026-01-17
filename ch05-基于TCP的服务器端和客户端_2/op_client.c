#include <arpa/inet.h>  // 提供 inet_addr、htons 等函数及 sockaddr_in 结构体定义（网络相关）
#include <stdio.h>  // 提供标准输入输出相关函数：printf、fputs、scanf、freopen（重定向文件）
#include <stdlib.h>      // 提供标准库函数：exit、atoi（字符串转整数）
#include <string.h>      // 提供字符串处理函数：memset（内存清空）
#include <sys/socket.h>  // 提供 socket、connect 等网络套接字相关函数
#include <unistd.h>      // 提供系统调用：read、write、close（I/O 操作）

#define BUF_SIZE 1024  // 发送缓冲区大小，用于存储客户端请求数据
#define RLT_SIZE 4     // 计算结果大小，这里为 int 类型，占用 4 字节
#define OPSZ 4         // 每个操作数占用字节数，这里假设是 4 字节（int 类型）
void error_handling(char* message);  // 错误处理函数声明

int main(int argc, char* argv[]) {
  // 将输入输出重定向到文件，这样可以批量测试而不需要交互式输入
  freopen("1.txt", "r", stdin);   // 输入重定向到 1.txt
  freopen("2.txt", "w", stdout);  // 输出重定向到 2.txt

  int sock;                     // 客户端套接字描述符
  char opmsg[BUF_SIZE];         // 存储操作数和运算符的缓冲区
  int result, opnd_cnt, i;      // 计算结果、操作数个数、循环计数
  struct sockaddr_in serv_adr;  // 服务器地址结构体（包含 IP 地址和端口号）

  // 参数检查：客户端运行时必须传入服务器的 IP 和端口号
  if (argc != 3) {
    printf("Usage : %s <IP> <port>\n",
           argv[0]);  // 提示用户正确的命令行参数格式
    exit(1);          // 如果参数不对，退出程序
  }

  // =========================
  // 创建 TCP 套接字
  // PF_INET：IPv4协议；SOCK_STREAM：面向连接的字节流（即 TCP 协议）
  // 返回的 sock 是一个文件描述符，用于后续的网络通信
  // 如果创建失败，返回 -1
  sock = socket(PF_INET, SOCK_STREAM, 0);
  if (sock == -1)
    error_handling(
        "socket() error");  // 出错则调用 error_handling 打印错误并退出

  // =========================
  // 初始化服务器地址结构
  memset(&serv_adr, 0, sizeof(serv_adr));  // 将服务器地址结构清零
  serv_adr.sin_family = AF_INET;           // 地址族设置为 IPv4
  serv_adr.sin_addr.s_addr =
      inet_addr(argv[1]);  // 设置服务器 IP 地址（从命令行参数获取）
  serv_adr.sin_port =
      htons(atoi(argv[2]));  // 设置服务器端口号，注意需要转换为网络字节序

  // =========================
  // 连接服务器
  // 调用 connect 尝试连接到指定的服务器地址
  // 如果连接失败，返回 -1
  if (connect(sock, (struct sockaddr*)&serv_adr, sizeof(serv_adr)) == -1)
    error_handling("connect() error!");  // 连接失败时输出错误信息并退出
  else
    puts("Connected...........\n");  // 如果连接成功，输出连接成功信息

  // =========================
  // 用户输入操作数数量
  fputs("Operand count: \n", stdout);  // 提示用户输入操作数个数
  scanf("%d", &opnd_cnt);  // 从标准输入读取操作数个数并存入 opnd_cnt

  // 按照协议约定：操作数个数占用 1 字节
  opmsg[0] = (char)opnd_cnt;  // 将操作数个数存入缓冲区的第一个字节

  // =========================
  // 用户依次输入每个操作数
  for (i = 0; i < opnd_cnt; i++) {
    printf("Operand %d: \n", i + 1);  // 提示用户输入第 i + 1 个操作数
    // 每个操作数占用 4 字节（int 类型），并且直接存入缓冲区
    // 注意：此处直接写入主机字节序，要求客户端和服务器的字节序一致
    //      但在不同的硬件平台（大小端问题）可能会有隐患，建议使用网络字节序
    scanf("%d",
          (int*)&opmsg[i * OPSZ + 1]);  // 将输入的操作数存入缓冲区的相应位置
    // 通过指针操作，直接将输入的数字存入缓冲区 opmsg
    // 中相应位置（避免多余的内存复制）
  }

  fgetc(stdin);  // 读取多余的换行符，防止后续读取时出现问题

  // =========================
  // 输入运算符（+、-、*）
  fputs("Operator: ", stdout);               // 提示用户输入运算符
  scanf("%c", &opmsg[opnd_cnt * OPSZ + 1]);  // 读取运算符并存入缓冲区，注意偏移

  // =========================
  // 消息格式：| 操作数个数(1B) | 操作数1(4B) | 操作数2(4B) | ... | 操作数N(4B)
  // | 运算符(1B) | 调用 write 将数据发送到服务器套接字
  // 发送的数据大小为：操作数个数(1B) + 操作数 * OPSZ + 运算符(1B)
  write(sock, opmsg, opnd_cnt * OPSZ + 2);  // 向服务器发送操作数和运算符

  // =========================
  // 读取 4 字节的结果并打印出来
  read(sock, &result, RLT_SIZE);              // 读取计算结果，存入 result 中
  printf("Operation result: %d \n", result);  // 输出计算结果

  // =========================
  // 关闭套接字，结束与服务器的连接
  close(sock);

  return 0;
}

// 错误处理函数：打印错误信息并退出程序
void error_handling(char* message) {
  fputs(message, stderr);  // 将错误信息输出到标准错误
  fputc('\n', stderr);     // 输出换行符
  exit(1);                 // 退出程序
}
