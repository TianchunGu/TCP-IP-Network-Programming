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
  //
  // ⚠️ 隐患1：Usage 打印不完整（没有显示 <IP> <port>），对用户不友好
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
  //
  // ⚠️ 隐患2：未检查 fopen 是否成功（可能返回 NULL：权限/路径/磁盘等问题）
  //   如果 fp==NULL，后续 fwrite/fclose 会崩溃或产生未定义行为
  //
  // ⚠️ 隐患3：文件名固定为 "received.data"
  //   - 会覆盖已有文件
  //   - 多次运行不便区分
  // -------------------------
  fp = fopen("received.data", "wb");

  // -------------------------
  // 初始化服务器地址
  // memset 清零避免未初始化字段干扰 connect
  // sin_family：IPv4
  // sin_addr.s_addr：将字符串 IP 转成网络序地址
  // sin_port：端口转网络字节序
  //
  // ⚠️ 隐患4：inet_addr 对非法 IP 返回 INADDR_NONE(0xFFFFFFFF)，且与
  // 255.255.255.255 有歧义 ⚠️ 隐患5：atoi 不做合法性检查，非数字会返回 0；端口 0
  // 基本不可用
  // -------------------------
  memset(&serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = inet_addr(argv[1]);
  serv_addr.sin_port = htons(atoi(argv[2]));

  // -------------------------
  // connect：连接到服务器
  //
  // ⚠️ 隐患6：未检查 connect 返回值（失败返回 -1）
  //   如果连接失败，后续 read/write 会失败，甚至造成逻辑异常
  // -------------------------
  connect(serv_sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr));

  // -------------------------
  // 循环从 socket 读取数据并写入文件
  // 对 TCP 来说：
  //   - read 可能读到任意长度（1..BUF_SIZE），不保证“按包”对齐
  //   - read 返回 0 表示对端关闭连接（EOF），通常意味着文件发送完毕
  //
  // ⚠️ 隐患7（非常关键）：read 出错会返回 -1，但 while 条件写成 != 0
  //   - 当 read_cnt == -1 时，(-1 != 0) 为真，循环会继续
  //   - 这样会导致死循环，并且 fwrite 会用 read_cnt=-1
  //   作为长度参数（被转换成非常大的 size_t），风险极大 实际工程必须区分：>0
  //   正常、==0 结束、<0 出错
  //
  // ⚠️ 隐患8：未检查 fwrite 的返回值
  //   - 磁盘满/写入失败时，数据会丢失但程序不报错
  //   - fwrite 可能“短写”（写入字节数少于
  //   read_cnt），需要循环补写（虽然对常规文件通常较少见，但依然是规范做法）
  // -------------------------
  while ((read_cnt = read(serv_sock, buf, BUF_SIZE)) != 0)
    fwrite((void*)buf, 1, read_cnt, fp);

  // -------------------------
  // 文件接收完毕（对端关闭连接导致 read 返回 0），打印提示
  //
  // ⚠️ 隐患9：字符串 "Received file date" 可能是笔误（data），但按要求不改代码
  // -------------------------
  puts("Received file date");

  // -------------------------
  // 向服务器发送确认消息
  // write 发送 10 字节："Thank you" 长度 9，加上末尾 '\0' 1 字节 = 10
  //
  // ⚠️ 隐患10：协议层面不一定需要发送 '\0'，发送 10
  // 字节会把字符串结束符也发出去；
  //   服务器如果按固定长度/文本协议解析，可能产生额外字符或不一致。
  //
  // ⚠️ 隐患11：未检查 write 返回值（失败 -1 或短写）
  //   对 TCP，write 可能只写入部分字节，需要循环发送完整数据。
  // -------------------------
  write(serv_sock, "Thank you", 10);

  // 关闭文件与 socket，释放资源
  // ⚠️ 隐患12：如果 fp 打开失败为 NULL，这里 fclose(fp) 会出错（见隐患2）
  fclose(fp);
  close(serv_sock);

  return 0;
}

void error_handling(char* message) {
  // 输出错误信息并退出程序
  // ⚠️ 这里没有输出 errno 细节（例如 perror），定位问题时信息可能不够
  fputs(message, stderr);
  fputc('\n', stderr);
  exit(1);
}
