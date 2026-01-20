#include <stdio.h>      // 标准输入输出：printf / puts / fputs / fgets 等
#include <stdlib.h>     // 标准库：exit / atoi 等
#include <string.h>     // 字符串/内存操作：memset / strcmp / strlen 等
#include <unistd.h>     // POSIX I/O：read / write / close 等
#include <arpa/inet.h>  // 网络地址与字节序：inet_addr / htons 等
#include <sys/socket.h> // 套接字 API：socket / connect 等

#define BUF_SIZE 1024               // 应用层缓冲区大小：一次最多读入/发送 1024 字节（含 '\0' 的空间）
void error_handling(char *message); // 错误处理函数声明：打印错误信息并退出

int main(int argc, char *argv[])
{
	int sock;                        // TCP 套接字文件描述符（客户端）
	char message[BUF_SIZE];          // 发送/接收缓冲区
	int str_len;                     // read() 实际读取到的字节数
	struct sockaddr_in serv_adr;     // 服务器 IPv4 地址结构体

	// -------------------- 命令行参数检查 --------------------
	// 需要提供两个参数：服务器 IP 与端口
	// argv[1] = IP（例如 127.0.0.1）
	// argv[2] = port（例如 9190）
	if(argc!=3) {
		printf("Usage : %s <IP> <port>\n", argv[0]);
		exit(1); // 参数不正确则退出
	}

	// -------------------- 创建 TCP 套接字 --------------------
	// PF_INET     : IPv4 协议族
	// SOCK_STREAM : 面向连接的字节流（TCP）
	// 0           : 让系统自动选择 TCP 协议（IPPROTO_TCP）
	sock=socket(PF_INET, SOCK_STREAM, 0);
	if(sock==-1)
		error_handling("socket() error"); // 创建失败则退出

	// -------------------- 填充服务器地址信息 --------------------
	// 先清零结构体，避免未初始化字段导致不可预期行为
	memset(&serv_adr, 0, sizeof(serv_adr));
	serv_adr.sin_family=AF_INET;                 // IPv4
	serv_adr.sin_addr.s_addr=inet_addr(argv[1]); // 将点分十进制 IP 字符串转为网络字节序的 32 位地址
	serv_adr.sin_port=htons(atoi(argv[2]));      // 端口：atoi 转 int，再 htons 转网络字节序（大端）

	// -------------------- 建立 TCP 连接 --------------------
	// connect 会触发 TCP 三次握手，成功后 sock 进入已连接状态
	// 之后可使用 read/write 在该连接上进行双向字节流通信
	if(connect(sock, (struct sockaddr*)&serv_adr, sizeof(serv_adr))==-1)
		error_handling("connect() error!");
	else
		puts("Connected..........."); // 连接成功提示

	// -------------------- 收发循环：简单回显客户端逻辑 --------------------
	while(1)
	{
		// 提示用户输入
		fputs("Input message(Q to quit): ", stdout);

		// 从标准输入读取一行到 message
		// 最多读取 BUF_SIZE-1 个字符，并自动添加 '\0'
		// 一般情况下会把换行符 '\n' 一并读入（缓冲区足够时）
		fgets(message, BUF_SIZE, stdin);

		// 若输入为 "q\n" 或 "Q\n" 则退出
		// 注意：比较时包含 '\n'，因为 fgets 会保留换行符
		if(!strcmp(message,"q\n") || !strcmp(message,"Q\n"))
			break;

		// write：将用户输入发送给服务器
		// strlen(message) 不包含结尾 '\0'，通常包含用户输入的 '\n'
		// 注意：TCP 是字节流协议，write 只保证“写入内核发送缓冲区”，不保证一次性发完（示例程序简化处理）
		write(sock, message, strlen(message));

		// read：从服务器读取响应
		// BUF_SIZE-1：预留 1 字节用于手动加 '\0'，便于按字符串输出
		// 返回值 str_len：
		//   >0：读取到的字节数
		//   =0：对端关闭连接（EOF）
		//   <0：出错（本示例未专门处理负值）
		str_len=read(sock, message, BUF_SIZE-1);

		// 将接收的数据当作 C 字符串打印时，需要手动添加结束符
		message[str_len]=0;

		// 打印服务器返回消息
		// 这里不额外打印 '\n'，因为服务器内容可能本身包含换行
		printf("Message from server: %s", message);
	}

	// -------------------- 关闭套接字 --------------------
	// close 会释放文件描述符，并触发 TCP 连接关闭流程（发送 FIN 等）
	close(sock);
	return 0;
}

void error_handling(char *message)
{
	// 将错误信息输出到标准错误流 stderr
	fputs(message, stderr);
	fputc('\n', stderr);
	exit(1); // 非正常退出
}
