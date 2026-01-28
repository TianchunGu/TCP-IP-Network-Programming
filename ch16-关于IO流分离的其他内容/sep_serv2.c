#include <stdio.h>      // 标准I/O：FILE, fdopen, fputs, fgets, fflush, fclose, stdout, fileno
#include <stdlib.h>     // atoi, exit（本程序未显式用 exit，但包含不改动）
#include <string.h>     // memset
#include <unistd.h>     // dup, close 等（dup 用于复制文件描述符）
#include <arpa/inet.h>  // htonl, htons：网络字节序转换
#include <sys/socket.h> // socket, bind, listen, accept, shutdown 等套接字接口
#define BUF_SIZE 1024   // 接收缓冲区大小：用于读客户端发送的一行文本

int main(int argc, char *argv[])
{
	int serv_sock, clnt_sock;
	/*
	 * serv_sock：服务器监听套接字（listen socket）
	 * - 负责绑定端口、监听连接请求
	 *
	 * clnt_sock：服务器已连接套接字（connected socket）
	 * - accept() 返回，用于和某个客户端进行实际数据收发
	 */

	FILE * readfp;
	FILE * writefp;
	/*
	 * readfp / writefp：标准 I/O 流（FILE*）
	 * - readfp：用于从客户端读取（fgets）
	 * - writefp：用于向客户端写（fputs）
	 *
	 * 教学重点：为什么这里要“读写分流”？
	 * - 标准I/O（stdio）带缓冲，并且读写切换需要格外小心
	 * - 将读与写分成两个 FILE* 可以让示例更直观
	 * - 但同一个底层 FD 被两个 FILE* 同时管理，会涉及“关闭/缓冲/半关闭”等细节
	 *   本代码通过 dup + shutdown 来更安全地处理这些问题（下面详解）
	 */

	struct sockaddr_in serv_adr, clnt_adr;
	/*
	 * serv_adr：服务器绑定用的地址（本地 IP + 端口）
	 * clnt_adr：accept 时由内核填充（客户端 IP + 端口）
	 */

	socklen_t clnt_adr_sz;
	/*
	 * clnt_adr_sz：clnt_adr 结构体大小
	 * - accept 的第三个参数是“输入输出参数”
	 *   调用前：告诉内核 clnt_adr 缓冲区有多大
	 *   调用后：内核写回实际使用的大小
	 */

	char buf[BUF_SIZE]={0,};
	/*
	 * buf：用于接收客户端最后回的一行数据
	 * 初始化为全 0 主要是为了调试/可读性，fgets 本身会写入并补 '\0'
	 */

	/*
	 * socket(PF_INET, SOCK_STREAM, 0)
	 * - PF_INET：IPv4
	 * - SOCK_STREAM：TCP（面向连接的字节流）
	 * - 0：自动选择协议（通常就是 TCP）
	 *
	 * 注意：本示例未检查 socket 返回值，真实工程中应检查是否为 -1
	 */
	serv_sock=socket(PF_INET, SOCK_STREAM, 0);

	/*
	 * memset 清零，避免结构体中出现未初始化字段
	 */
	memset(&serv_adr, 0, sizeof(serv_adr));

	/*
	 * 设置 IPv4 地址族
	 */
	serv_adr.sin_family=AF_INET;

	/*
	 * INADDR_ANY 表示 0.0.0.0：绑定本机所有网卡/所有 IP
	 * htonl：host to network long，把 32 位整数转成网络字节序（大端）
	 */
	serv_adr.sin_addr.s_addr=htonl(INADDR_ANY);

	/*
	 * 设置端口（网络字节序）：
	 * atoi(argv[1])：字符串端口 -> 整数
	 * htons：host to network short，把 16 位端口转成网络字节序
	 *
	 * 注意：本程序未检查 argc 是否足够，若没传端口参数会越界访问 argv[1]
	 */
	serv_adr.sin_port=htons(atoi(argv[1]));

	/*
	 * bind：把监听 socket 绑定到本地 IP:端口
	 * - 没有 bind 就无法在指定端口提供服务
	 * - 常见失败：端口被占用、权限不足等
	 * 本程序未检查返回值，真实工程应检查
	 */
	bind(serv_sock, (struct sockaddr*) &serv_adr, sizeof(serv_adr));

	/*
	 * listen：进入监听状态
	 * - 第二参数 5 是 backlog：等待 accept 的连接请求队列上限
	 */
	listen(serv_sock, 5);

	/*
	 * accept：阻塞等待一个客户端连接
	 * - 成功后返回一个新的已连接 socket：clnt_sock
	 * - clnt_adr 保存对方地址信息
	 * 本程序只 accept 一次 -> 只服务一个客户端
	 */
	clnt_adr_sz=sizeof(clnt_adr);
	clnt_sock=accept(serv_sock, (struct sockaddr*)&clnt_adr,&clnt_adr_sz);

	/*
	 * fdopen(clnt_sock, "r")：
	 * - 用 clnt_sock 创建一个“读流”readfp
	 * - 方便用 fgets 按行读取客户端发送的文本
	 */
	readfp=fdopen(clnt_sock, "r");

	/*
	 * writefp=fdopen(dup(clnt_sock), "w");
	 *
	 * 教学重点：为什么这里要 dup(clnt_sock)？
	 *
	 * 1) dup 会复制一个新的文件描述符（FD）
	 *    - 新 FD 和旧 FD 指向同一个底层 socket（共享同一连接）
	 *    - 但 FD 数值不同（相当于多了一把“钥匙”）
	 *
	 * 2) 好处：避免“同一个 FD 被两个 FILE* 管理”引发的麻烦
	 *    - 如果直接对 clnt_sock 同时 fdopen(...,"r") 和 fdopen(...,"w")
	 *      那么 fclose(readfp) / fclose(writefp) 都可能去 close 同一个底层 FD，
	 *      造成重复关闭或缓冲刷新时序问题
	 *    - 通过 dup 得到第二个 FD 后：
	 *      readfp 和 writefp 各自拥有不同的底层 FD，
	 *      这样 fclose 各自关闭自己的 FD，不会“争抢同一个 FD”
	 *
	 * 3) 仍然要理解：它们底层连接是同一个 TCP 连接
	 *    - 复制的是“描述符”，不是“新连接”
	 */
	writefp=fdopen(dup(clnt_sock), "w");

	/*
	 * 服务器向客户端发送三行文本（文本行协议风格）：
	 * - fputs 写入 stdio 缓冲区，不一定立刻发到网络
	 * - 因此后面要 fflush(writefp) 强制发送
	 */
	fputs("FROM SERVER: Hi~ client? \n", writefp);
	fputs("I love all of the world \n", writefp);
	fputs("You are awesome! \n", writefp);

	/*
	 * fflush：把 writefp 的用户态缓冲立即刷新到底层 socket
	 * - 否则客户端可能收不到，直到缓冲满或 fclose 才发出
	 */
	fflush(writefp);

	/*
	 * shutdown(fileno(readfp), SHUT_WR);
	 *
	 * 教学重点：shutdown 和 close/fclose 的区别
	 *
	 * - close/fclose：关闭“文件描述符/流”，释放资源，FD 不再可用
	 * - shutdown：对 socket 连接做“半关闭”（half-close），控制收/发方向
	 *
	 * SHUT_WR 的意义：
	 * - 关闭“发送方向”（写方向）
	 * - 内核会发送 FIN，告诉对端：“我这边不会再发送数据了”
	 * - 但“接收方向”（读方向）仍然保持打开（可以继续读对方发来的数据）
	 *
	 * 为什么要半关闭？
	 * - 这是一种常见的“协议语义”：我发完了，你可以开始回我最后的消息
	 * - 对端收到 FIN 后，知道服务器发送结束，便可进行相应处理并回传数据
	 *
	 * 注意：这里传入的是 fileno(readfp)
	 * - fileno(readfp) 取得 readfp 对应的底层 FD（也就是 clnt_sock）
	 * - 虽然是 readfp，但它底层仍然是一个 socket FD，
	 *   shutdown 作用于 socket 本身，与“你用它读还是写”无关
	 *
	 * 也可以用下面注释的那一行 shutdown(fileno(writefp), SHUT_WR)：
	 * - 因为 writefp 的底层 FD 是 dup 得到的新 FD，但同样指向同一 socket 连接
	 * - 对同一连接而言，对任意一个引用该连接的 FD 调用 shutdown 都会影响该连接
	 */
	shutdown(fileno(readfp), SHUT_WR); // 注意这一行，也可以改成下面注释的那一行。
	// shutdown(fileno(writefp), SHUT_WR);

	/*
	 * fclose(writefp)：
	 * - 关闭写流，释放 FILE* 资源，并关闭它底层的 FD（dup 出来的那个 FD）
	 *
	 * 教学提示：
	 * - 由于前面已经 shutdown 写方向，这里 fclose 更像是资源回收
	 * - 即使不 fclose，程序结束也会回收，但规范写法应关闭
	 */
	fclose(writefp);

	/*
	 * 从客户端读取一行并打印到服务器终端：
	 * fgets：按行读取，遇到 '\n' 或缓冲区满停止，并以 '\0' 结尾
	 * fputs 到 stdout：输出到屏幕
	 *
	 * 隐含假设：
	 * - 客户端会发送以 '\n' 结束的一行文本，否则 fgets 可能等待更多数据
	 */
	fgets(buf, sizeof(buf), readfp);
	fputs(buf, stdout);

	/*
	 * fclose(readfp)：
	 * - 关闭读流，释放资源，并关闭底层 FD（原始 clnt_sock）
	 * - 读写分别拥有不同 FD（因为写端使用了 dup），因此不会发生“重复关闭同一 FD”的问题
	 */
	fclose(readfp);

	return 0; // 程序正常结束
}
