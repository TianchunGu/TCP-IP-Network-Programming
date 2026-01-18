#include <stdio.h>      // 标准输入输出：printf / puts / fputs / fgets 等
#include <stdlib.h>     // 标准库：exit / atoi 等
#include <string.h>     // 字符串/内存操作：memset / strcmp / strlen 等
#include <unistd.h>     // POSIX I/O：read / write / close 等
#include <arpa/inet.h>  // 网络地址转换：inet_addr / htons 等
#include <sys/socket.h> // 套接字 API：socket / connect 等

#define BUF_SIZE 1024              // 应用层缓冲区大小：一次最多读取/发送 1024 字节（含结尾 '\0' 的空间）
void error_handling(char *message); // 错误处理函数声明：打印错误信息并退出

int main(int argc, char *argv[])
{
	int sock;                        // TCP 套接字文件描述符（客户端）
	char message[BUF_SIZE];          // 发送/接收缓冲区
	int str_len;                     // 实际 read() 读到的字节数
	struct sockaddr_in serv_adr;     // 服务器地址结构体（IPv4）

	// 参数检查：需要提供服务器 IP 和端口
	// argv[1] = IP（如 127.0.0.1），argv[2] = port（如 9190）
	if(argc!=3) {
		printf("Usage : %s <IP> <port>\n", argv[0]);
		exit(1); // 参数不足/过多则退出
	}

	// 创建 TCP 套接字：
	// PF_INET     : IPv4
	// SOCK_STREAM : 面向连接的字节流（TCP）
	// 0           : 让系统自动选择 TCP 协议（IPPROTO_TCP）
	sock=socket(PF_INET, SOCK_STREAM, 0);
	if(sock==-1)
		error_handling("socket() error"); // 创建失败则退出

	// 将服务器地址结构体清零，避免未初始化字段引发问题
	memset(&serv_adr, 0, sizeof(serv_adr));
	serv_adr.sin_family=AF_INET;                 // IPv4
	serv_adr.sin_addr.s_addr=inet_addr(argv[1]); // 将点分十进制 IP 字符串转为网络字节序的 IPv4 地址
	serv_adr.sin_port=htons(atoi(argv[2]));      // 端口：atoi 转 int，再 htons 转网络字节序（大端）

	// connect：与服务器建立 TCP 连接（会触发三次握手）
	// 成功后该 sock 就成为一条已连接的 TCP 通道，可用 read/write 收发字节流
	if(connect(sock, (struct sockaddr*)&serv_adr, sizeof(serv_adr))==-1)
		error_handling("connect() error!");
	else
		puts("Connected..........."); // 连接成功提示

	// 主循环：从用户读取一行文本，发送给服务器，再读取服务器响应并打印
	while(1)
	{
		// 提示用户输入；stdout 用 fputs 更便于控制是否换行
		fputs("Input message(Q to quit): ", stdout);

		// 从标准输入读取一行到 message
		// 最多读取 BUF_SIZE-1 个字符，并自动补 '\0'
		// 通常会把换行符 '\n' 也读进来（缓冲区足够时）
		fgets(message, BUF_SIZE, stdin);

		// 若输入为 "q\n" 或 "Q\n" 则退出循环
		// 注意：因为 fgets 会把 '\n' 读进来，所以比较字符串时也包含 '\n'
		if(!strcmp(message,"q\n") || !strcmp(message,"Q\n"))
			break;

		// write：向已连接的 TCP 套接字发送数据
		// strlen(message) 不包含字符串结尾的 '\0'，但通常包含用户输入的 '\n'
		// 注意：TCP 是字节流协议，write 的长度不保证一次性完整发送到对端应用层（但在简单示例里常这么写）
		write(sock, message, strlen(message));

		// read：从 TCP 套接字读取数据到缓冲区
		// BUF_SIZE-1：预留一个字节用于手动添加字符串结束符 '\0'
		// 返回值为实际读到的字节数：
		// - >0 表示成功读取
		// - 0 表示对端关闭连接（EOF）
		// - -1 表示出错
		str_len=read(sock, message, BUF_SIZE-1);

		// 将接收到的数据当作 C 字符串打印时，需要手动补 '\0'
		message[str_len]=0;

		// 输出服务器返回的数据
		// 这里不额外加 '\n'，因为服务器返回内容可能自带换行
		printf("Message from server: %s", message);
	}

	// 关闭套接字，释放资源（同时向对端触发 TCP 连接关闭流程）
	close(sock);
	return 0;
}

void error_handling(char *message)
{
	// 输出错误信息到标准错误流 stderr
	fputs(message, stderr);
	fputc('\n', stderr);
	exit(1); // 非正常退出
}
