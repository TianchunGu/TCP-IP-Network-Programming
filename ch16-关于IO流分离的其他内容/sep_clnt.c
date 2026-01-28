#include <stdio.h>      // 标准I/O：FILE, fgets, fputs, fflush, stdout, fdopen, fclose
#include <stdlib.h>     // atoi, exit（本程序未用 exit，但包含不改动）
#include <string.h>     // memset
#include <unistd.h>     // POSIX：close 等（本程序未直接调用 close，但 socket FD 本质是文件描述符）
#include <arpa/inet.h>  // inet_addr, htons 等：IP字符串转地址、字节序转换
#include <sys/socket.h> // socket, connect 等套接字相关系统调用
#define BUF_SIZE 1024   // 应用层缓冲区大小：用于接收服务器发送来的文本数据（按行）

int main(int argc, char *argv[])
{
	int sock;
	/*
	 * sock：socket() 返回的“套接字文件描述符”（FD）
	 * - 在 UNIX/Linux 中，socket 也用整数 FD 表示，和文件、管道一样通过 read/write 操作
	 * - 这里后续会用 fdopen 把它包装成 FILE*，以便使用 fgets/fputs 进行按行读写
	 */

	char buf[BUF_SIZE];
	/*
	 * buf：接收缓冲区
	 * - 这里用 fgets 从网络流中读取“文本行”
	 * - 由于 fgets 是按行读取的，程序隐含假设：服务器发送的数据包含 '\n' 换行符
	 */

	struct sockaddr_in serv_addr;
	/*
	 * serv_addr：服务器地址信息（IPv4）
	 * - 需要填入：地址族 AF_INET、IP 地址、端口号
	 * - connect() 会使用该结构体确定要连接到哪台服务器、哪个端口
	 */

	FILE *readfp;
	FILE *writefp;
	/*
	 * readfp / writefp：标准 I/O 流指针（FILE*）
	 * - readfp：从服务器读取数据的流（"r"）
	 * - writefp：向服务器写入数据的流（"w"）
	 *
	 * 教学重点：为什么要用 FILE*？
	 * - 标准I/O提供按行读取的 fgets，写字符串的 fputs，更方便做“文本协议”
	 * - 但标准I/O带缓冲，因此写入后通常需要 fflush 才能保证数据立即发出
	 */

	/*
	 * socket(PF_INET, SOCK_STREAM, 0)
	 * - PF_INET：IPv4 协议族
	 * - SOCK_STREAM：面向连接的字节流服务 -> TCP
	 * - 0：让系统自动选择协议（对 PF_INET + SOCK_STREAM 通常就是 TCP）
	 *
	 * 注意：本程序没有检查 socket 是否返回 -1，也没有检查参数个数 argc
	 * - 若 argv[1]/argv[2] 不存在，会发生越界访问
	 * - 若 socket/connect 失败，后续行为可能异常
	 * 题目要求不改代码，因此只在注释中说明风险点。
	 */
	sock = socket(PF_INET, SOCK_STREAM, 0);

	/*
	 * memset：将地址结构体清零
	 * - 防止未初始化字段造成不可预测的问题
	 */
	memset(&serv_addr, 0, sizeof(serv_addr));

	/*
	 * 设置服务器地址族为 IPv4
	 */
	serv_addr.sin_family = AF_INET;

	/*
	 * inet_addr(argv[1])：将点分十进制 IP 字符串转换为 32 位网络序 IPv4 地址
	 * - argv[1] 形如 "127.0.0.1"
	 * - 返回值直接适配 sin_addr.s_addr（其要求网络字节序）
	 *
	 * 教学补充：
	 * - inet_addr 是较老的接口，现代常用 inet_pton
	 * - 但此处不改代码
	 */
	serv_addr.sin_addr.s_addr = inet_addr(argv[1]);

	/*
	 * htons(atoi(argv[2]))：设置端口号（网络字节序）
	 * - argv[2] 为端口字符串，例如 "8080"
	 * - atoi：字符串转整数（主机字节序）
	 * - htons：host to network short，把 16 位端口转为网络字节序（大端序）
	 */
	serv_addr.sin_port = htons(atoi(argv[2]));

	/*
	 * connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr))
	 * - 客户端发起连接请求，与服务器进行 TCP 三次握手
	 * - 成功返回 0，失败返回 -1
	 *
	 * connect 成功后：
	 * - sock 变成一个“已连接 socket”，可以用它与服务器收发数据
	 *
	 * 注意：本程序没有检查 connect 返回值，若连接失败，后续 fdopen/fgets 可能出错
	 */
	connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr));

	/*
	 * fdopen(sock, "r"/"w")：将 socket 的文件描述符包装为标准I/O流
	 * - 这样就可以用 fgets/fputs 进行按行的文本读写
	 *
	 * 教学重点：stdio 缓冲机制
	 * - 写入 writefp 时，数据通常先进入用户态缓冲区，不一定立即发送到网络
	 * - 所以每次 fputs 后一般要 fflush(writefp) 来强制发送
	 *
	 * 额外提醒（更深入）：
	 * - 这里对同一个 FD 调用了两次 fdopen（一个 "r" 一个 "w"）
	 * - 在示例代码中常见，但在严格工程实践里可能会涉及缓冲/关闭的细节风险
	 *   （例如两个 FILE* 都可能在 fclose 时关闭同一个底层 FD）
	 * - 本程序最后 fclose(writefp) 和 fclose(readfp) 可能导致“重复关闭同一 FD”的风险
	 * - 题目要求不改代码，这里只说明原理
	 */
	readfp = fdopen(sock, "r");
	writefp = fdopen(sock, "w");

	while (1)
	{
		/*
		 * fgets(buf, sizeof(buf), readfp)
		 * - 从 readfp 中读取一行文本到 buf
		 * - 最多读 sizeof(buf)-1 个字符，保证以 '\0' 结尾
		 * - 若读到 '\n' 会把 '\n' 也读入 buf
		 *
		 * 返回值：
		 * - 成功读到至少一个字符：返回 buf（非 NULL）
		 * - 失败/或对端关闭连接且无数据可读：返回 NULL
		 *
		 * 对 socket 而言，何时会返回 NULL？
		 * - 当服务器关闭连接（发送 FIN）后，底层 read 返回 0，stdio 置 EOF
		 * - fgets 便会返回 NULL，于是这里 break 退出循环
		 */
		if (fgets(buf, sizeof(buf), readfp) == NULL)
			break;

		/*
		 * fputs(buf, stdout)：把从服务器收到的一行输出到屏幕
		 * - stdout 在连接到终端时通常是“行缓冲”（遇到 '\n' 会自动刷新）
		 * - 但如果 stdout 被重定向到文件，则可能是“全缓冲”，不一定立即写出
		 * 因此这里显式 fflush(stdout) 以保证立刻显示
		 */
		fputs(buf, stdout);
		fflush(stdout);
	}

	/*
	 * 当服务器不再发送数据/关闭连接后，跳出循环
	 * 接下来客户端向服务器发送最后一句固定文本：
	 */
	fputs("FROM CLIENT: Thank you! \n", writefp);

	/*
	 * fflush(writefp)：强制将 stdio 的写缓冲刷新到 socket
	 * - 否则这句“Thank you”可能仍停留在用户态缓冲区中，导致服务器收不到
	 */
	fflush(writefp);

	/*
	 * fclose(writefp)/fclose(readfp)：关闭标准I/O流
	 * - fclose 会刷新缓冲并释放资源
	 * - 同时通常会关闭底层 FD（socket）
	 *
	 * 教学提示：
	 * - 这里两个 fclose 可能会重复关闭同一个 sock（因为两个流共享同一 FD）
	 * - 在示例程序里常能跑通，但严格写法通常会避免这种“双重关闭”风险
	 */
	fclose(writefp);
	fclose(readfp);

	return 0; // 正常结束
}
