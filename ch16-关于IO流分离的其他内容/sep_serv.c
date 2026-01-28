#include <stdio.h>      // 标准I/O：FILE, fdopen, fputs, fgets, fflush, fclose, stdout
#include <stdlib.h>     // atoi, exit（本程序未显式用 exit，但包含不改动）
#include <string.h>     // memset
#include <unistd.h>     // close 等（本程序未直接 close，但 socket FD 属于 POSIX FD）
#include <arpa/inet.h>  // htonl, htons 等网络字节序转换
#include <sys/socket.h> // socket, bind, listen, accept 等套接字系统调用
#define BUF_SIZE 1024   // 接收缓冲区大小：用于读取客户端最终发来的文本行

int main(int argc, char *argv[])
{
	int serv_sock, clnt_sock;
	/*
	 * serv_sock：服务器“监听套接字”（listen socket）
	 * - 用于绑定端口并监听连接请求
	 *
	 * clnt_sock：服务器“已连接套接字”（connected socket）
	 * - accept() 成功后返回，用于与某一个客户端进行实际收发数据
	 */

	FILE *readfp;
	FILE *writefp;
	/*
	 * readfp / writefp：把 clnt_sock 包装成标准I/O流（FILE*）
	 * - readfp：以 "r" 模式读取客户端数据（方便用 fgets 按行读取）
	 * - writefp：以 "w" 模式向客户端发送数据（方便用 fputs 写字符串）
	 *
	 * 教学重点：标准 I/O（stdio）与 socket 的结合
	 * - fdopen 可以把“文件描述符”变成 FILE*，从而用更高层的 fgets/fputs
	 * - 但 stdio 具有缓冲机制：写数据可能先进入用户态缓冲区，需要 fflush 才保证立刻发出
	 */

	struct sockaddr_in serv_adr, clnt_adr;
	/*
	 * serv_adr：服务器地址结构体（用于 bind 绑定本地地址与端口）
	 * clnt_adr：客户端地址结构体（accept 时由内核填充，记录对方 IP/端口）
	 */

	socklen_t clnt_adr_sz;
	/*
	 * clnt_adr_sz：地址结构体长度
	 * - accept 的第三个参数是“输入输出参数”
	 *   调用前：传入 clnt_adr 缓冲区大小
	 *   调用后：内核写回实际地址长度
	 */

	char buf[BUF_SIZE] = {
		0,
	};
	/*
	 * buf：接收缓冲区，用于最后从客户端读取一行数据
	 * 初始化为全 0 的原因：
	 * - 让缓冲区初始内容可控，避免调试时看到“旧垃圾值”
	 * - 但对 fgets 来说不是必须的，因为 fgets 会写入并以 '\0' 结尾
	 */

	/*
	 * socket(PF_INET, SOCK_STREAM, 0)
	 * - PF_INET：IPv4 协议族
	 * - SOCK_STREAM：面向连接的字节流 -> TCP
	 * - 0：让系统自动选择协议（通常是 TCP）
	 *
	 * 注意：本代码没有检查 socket 是否失败（返回 -1）
	 * 题目要求不改动代码，这里只在注释中说明：真实程序应检查返回值并处理错误。
	 */
	serv_sock = socket(PF_INET, SOCK_STREAM, 0);

	/*
	 * 将服务器地址结构体清零，避免未初始化字段导致不可预期行为
	 */
	memset(&serv_adr, 0, sizeof(serv_adr));

	/*
	 * 设置地址族为 IPv4
	 */
	serv_adr.sin_family = AF_INET;

	/*
	 * htonl(INADDR_ANY)：
	 * - INADDR_ANY 表示 0.0.0.0，即“绑定到本机所有网卡/所有本地IP”
	 *   这样客户端只要访问本机任意 IP 的该端口，连接都能到达该服务
	 *
	 * - htonl：host to network long，把 32 位整数从主机字节序转为网络字节序（大端）
	 *   网络协议字段（IP地址、端口）要求使用网络字节序存储/传输
	 */
	serv_adr.sin_addr.s_addr = htonl(INADDR_ANY);

	/*
	 * htons(atoi(argv[1]))：
	 * - argv[1]：命令行传入的端口字符串，例如 "8080"
	 * - atoi：转为整数（主机字节序）
	 * - htons：host to network short，把 16 位端口转为网络字节序（大端）
	 *
	 * 注意：本程序未检查 argc 是否足够，若未提供端口参数会访问 argv[1] 越界
	 * 题目要求不改代码，这里仅说明风险。
	 */
	serv_adr.sin_port = htons(atoi(argv[1]));

	/*
	 * bind(serv_sock, ...)：
	 * - 把监听 socket 与“本地IP:端口”绑定
	 * - 只有 bind 成功，服务器才能在该端口上接收连接请求
	 *
	 * 常见失败原因：
	 * - 端口被占用（Address already in use）
	 * - 权限不足（绑定 <1024 端口通常需要管理员权限）
	 *
	 * 注意：本代码没有检查 bind 返回值（失败时会继续执行，导致后续行为不可控）
	 */
	bind(serv_sock, (struct sockaddr *)&serv_adr, sizeof(serv_adr));

	/*
	 * listen(serv_sock, 5)：
	 * - 将 serv_sock 变为“监听状态”
	 * - 第二个参数 5 是 backlog：连接请求排队的最大长度
	 *
	 * 教学提示：
	 * - backlog 不是“最多客户端数”，而是“还没被 accept 取走的连接请求队列长度上限”
	 * - 本代码同样没有检查 listen 返回值
	 */
	listen(serv_sock, 5);

	/*
	 * accept 准备：
	 * clnt_adr_sz 初始化为 clnt_adr 缓冲区大小
	 */
	clnt_adr_sz = sizeof(clnt_adr);

	/*
	 * accept(serv_sock, ...)：
	 * - 阻塞等待客户端连接
	 * - 当有客户端完成 TCP 三次握手后，accept 返回一个新的已连接 socket：clnt_sock
	 * - clnt_adr 会被填充为客户端地址信息
	 *
	 * 教学重点：监听 socket 与已连接 socket 的分工
	 * - serv_sock：负责“接客”（接收连接请求）
	 * - clnt_sock：负责“服务某个具体客户端”（收发数据）
	 *
	 * 本示例只 accept 一次，表示只服务一个客户端后程序就结束（非并发、非循环）
	 */
	clnt_sock = accept(serv_sock, (struct sockaddr *)&clnt_adr, &clnt_adr_sz);

	/*
	 * 将 clnt_sock（FD）包装成标准I/O流：
	 * - 读流 readfp：用于 fgets（按行读取）
	 * - 写流 writefp：用于 fputs（写字符串）
	 *
	 * 重要提醒（更深入的教学点）：
	 * - 同一个 FD 被 fdopen 两次分别得到 readfp/writefp
	 * - fclose(readfp)/fclose(writefp) 都可能关闭同一个底层 FD
	 * - 严格工程写法通常会避免这种潜在的“双重关闭”风险（例如 dup 后再 fdopen）
	 * - 本题要求不改代码，因此只通过注释提示
	 */
	readfp = fdopen(clnt_sock, "r");
	writefp = fdopen(clnt_sock, "w");

	/*
	 * 向客户端连续发送三行文本：
	 * - fputs 只负责把字符串写入 stdio 缓冲区（不一定立刻发到网络）
	 * - 这里每行都带 '\n'，属于“文本行协议”风格
	 */
	fputs("FROM SERVER: Hi~ client? \n", writefp);
	fputs("I love all of the world \n", writefp);
	fputs("You are awesome! \n", writefp);

	/*
	 * fflush(writefp)：
	 * - 强制刷新 stdio 输出缓冲区
	 * - 确保前面三行立即通过 socket 发送给客户端
	 * 否则可能出现：客户端迟迟收不到数据，直到缓冲区满或 fclose 才被发送
	 */
	fflush(writefp);

	/*
	 * fclose(writefp)：
	 * - 关闭写流，通常会关闭底层 socket 的“写端”
	 * - 对 TCP 而言，这可能触发发送 FIN，告诉对端“我不再发送数据了”
	 *
	 * 教学提示：
	 * - 在半关闭（half-close）模型中，关闭写端并不一定意味着无法再读
	 * - 本程序关闭 writefp 后仍继续从 readfp 读客户端数据，体现“先发后收”的流程
	 * - 但由于两个 FILE* 共享同一 FD，实现/平台差异可能影响行为（示例常见写法）
	 */
	fclose(writefp);

	/*
	 * 从客户端读取一行数据：
	 * fgets(buf, sizeof(buf), readfp)
	 * - 最多读 BUF_SIZE-1 个字符，保证以 '\0' 结束
	 * - 若遇到 '\n' 会把 '\n' 也读入 buf
	 *
	 * 隐含假设：
	 * - 客户端会发送带 '\n' 的一行文本，否则 fgets 可能等待更多数据
	 */
	fgets(buf, sizeof(buf), readfp);

	/*
	 * 将客户端发来的这一行打印到服务器控制台：
	 * - stdout 可能带缓冲；但一般在终端上遇到 '\n' 会自动刷新
	 * - 这里没有 fflush(stdout)，但通常也能立刻看到输出
	 */
	fputs(buf, stdout);

	/*
	 * 关闭读流：
	 * - 释放 FILE* 资源，并通常关闭底层 FD（socket）
	 * - 若 writefp 已经 fclose 关闭过底层 FD，这里再次 fclose 可能触发重复关闭的风险
	 * - 题目要求不改代码，因此只说明潜在问题
	 */
	fclose(readfp);

	return 0; // 程序正常结束
}
