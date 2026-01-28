/*
 * 这是一个 TCP 客户端程序：
 * 1) 从命令行接收服务器 IP 和端口；
 * 2) 创建 TCP socket 并连接服务器；
 * 3) 将 socket 文件描述符转换为标准 I/O 流（FILE*）；
 * 4) 循环读取用户输入 -> 发送给服务器 -> 接收服务器回显/响应 -> 打印；
 * 5) 输入 q/Q 退出，关闭流并结束程序。
 */

#include <stdio.h>      // printf, puts, fputs, fgets, FILE, fdopen, fclose, stderr, stdout, stdin
#include <stdlib.h>     // exit, atoi
#include <string.h>     // memset, strcmp
#include <unistd.h>     // POSIX API（本例中主要用于通用 UNIX 环境；socket FD 也是 UNIX 的文件描述符）
#include <arpa/inet.h>  // inet_addr, htons, htonl 等与网络字节序/地址转换相关的函数
#include <sys/socket.h> // socket, connect 以及 socket 相关结构与常量（PF_INET, SOCK_STREAM 等）

#define BUF_SIZE 1024   // 应用层缓冲区大小：用于保存用户输入与服务器返回的数据
void error_handling(char *message); // 错误处理函数声明：打印错误并退出程序

int main(int argc, char* argv[])
{
    int serv_sock;                 // 保存 socket() 返回的“套接字文件描述符”
    struct sockaddr_in serv_addr;  // IPv4 地址结构体，用于保存服务器地址信息（IP、端口等）
    char message[BUF_SIZE];        // 消息缓冲区：用于读写数据

    /*
     * 参数检查：
     * argv[0] 为程序名
     * argv[1] 为服务器IP（点分十进制字符串，如 "127.0.0.1"）
     * argv[2] 为端口号（字符串形式，如 "8080"）
     */
    if(argc != 3)
    {
        printf("Usage: %s <IP> <port>\n", argv[0]);
        exit(1); // 参数不对属于“无法继续运行”的情况，直接退出
    }

    /*
     * 创建套接字：
     * PF_INET：IPv4 协议族（Protocol Family）
     * SOCK_STREAM：面向连接的字节流 -> TCP
     * 0：让系统根据 PF_INET + SOCK_STREAM 自动选择协议（TCP）
     *
     * 返回值：>=0 表示成功（文件描述符），-1 表示失败
     */
    serv_sock = socket(PF_INET, SOCK_STREAM, 0);
    if(serv_sock == -1)
        error_handling("socket() error");
    
    /*
     * 初始化服务器地址结构体：
     * memset 置 0，避免结构体中未初始化字段带来不可预期行为
     */
    memset(&serv_addr, 0, sizeof(serv_addr));

    /*
     * 填写 sockaddr_in：
     * sin_family：地址族，必须与 socket 的 PF_INET 对应（这里用 AF_INET）
     */
    serv_addr.sin_family = AF_INET;

    /*
     * sin_addr.s_addr：32位IPv4地址（网络字节序）
     * inet_addr(argv[1])：把点分十进制 IP 字符串转换为 32 位网络序地址
     * 注意：inet_addr 已相对老旧，现代也可用 inet_pton，但这里不改代码
     */
    serv_addr.sin_addr.s_addr = inet_addr(argv[1]);

    /*
     * sin_port：端口号（网络字节序）
     * atoi(argv[2])：把端口字符串转为整数
     * htons(...)：host to network short，将主机字节序转换为网络字节序（16位端口）
     */
    serv_addr.sin_port = htons(atoi(argv[2]));

    /*
     * connect：发起 TCP 连接（三次握手）
     * 参数：
     *  - serv_sock：客户端 socket FD
     *  - (struct sockaddr*)&serv_addr：通用地址结构指针（将 sockaddr_in 转为 sockaddr）
     *  - sizeof(serv_addr)：地址结构体长度
     *
     * 返回值：0 成功；-1 失败（例如服务器不可达、端口未监听等）
     */
    if(connect(serv_sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == -1)
        error_handling("connect() error");
    else
        puts("Connected...");
    
    /*
     * fdopen：将“文件描述符 FD”包装成“标准 I/O 流 FILE*”
     * 这样可以使用 fgets/fputs 这类更方便的 stdio 函数来读写 socket
     *
     * 注意点（重要）：
     * - 这里对同一个 FD 调用了两次 fdopen，分别用 "r" 和 "w" 得到读流和写流
     * - 这种做法在很多示例中可行，但要注意缓冲与关闭行为，避免重复关闭/缓冲冲突
     * - 本代码最后分别 fclose(writefp) 和 fclose(readfp)，等价于关闭同一个 FD 两次的风险
     *   但在某些实现/情况下可能仍能工作（示例代码常见写法）
     *   更稳妥的做法通常是 dup() 后再 fdopen，或只使用一个 "r+" 流，但这里不改代码
     */
    FILE* readfp = fdopen(serv_sock, "r");  // 从 socket 读数据的流（文本模式）
    FILE* writefp = fdopen(serv_sock, "w"); // 向 socket 写数据的流（文本模式）

    while(1)
    {
        /*
         * 提示用户输入：
         * fputs 输出到 stdout（标准输出）
         */
        fputs("Input message(q/Q to quit): ", stdout);

        /*
         * 从标准输入读取一行到 message：
         * fgets 会读取最多 BUF_SIZE-1 个字符，并以 '\0' 结尾
         * 如果读到换行符 '\n' 也会被保留在 message 中
         */
        fgets(message, BUF_SIZE, stdin);

        /*
         * 判断退出条件：
         * 用户输入为 "q\n" 或 "Q\n" 则退出循环
         * （因为 fgets 会把回车换行读进来，所以比较 "q\n" 而不是 "q"）
         */
        if(!strcmp(message, "q\n") || !strcmp(message, "Q\n"))
            break;

        /*
         * 将用户输入发送给服务器：
         * fputs 将 message 写入 writefp（socket 的写流）
         * 注意：stdio 是带缓冲的，不一定立刻发出去
         */
        fputs(message, writefp);

        /*
         * fflush 强制刷新输出缓冲区：
         * 让数据立即从缓冲区写到 socket
         * 对于交互式客户端很关键，否则服务器可能收不到数据或延迟很久才收到
         */
        fflush(writefp);

        /*
         * 从服务器读取一行响应：
         * 这里假设服务器也会以“按行”方式返回数据（包含 '\n' 或可被 fgets 截断）
         * fgets 从 readfp 读取，直到读到 '\n' 或达到 BUF_SIZE-1 或 EOF
         */
        fgets(message, BUF_SIZE, readfp);

        /*
         * 打印服务器返回的消息：
         * message 已经包含 '\n' 时，这里 printf 后又加 "\n" 会出现双换行
         * 但这属于显示效果问题，不改代码
         */
        printf("Message from server: %s\n", message);
    }

    /*
     * 关闭流：
     * fclose 会关闭 FILE*，并在内部调用 close(fd)
     * 本代码对同一个 FD 包装成两个 FILE*，分别 fclose 可能导致重复 close
     * 但由于是示例代码，保持不改动，只在注释中提醒风险
     */
    fclose(writefp);
    fclose(readfp);

    return 0;
}

/*
 * error_handling：统一错误处理
 * - 将错误信息输出到标准错误 stderr
 * - 输出换行
 * - 退出程序（非 0 表示异常退出）
 */
void error_handling(char *message)
{
	fputs(message, stderr);  // 输出错误信息到 stderr
	fputc('\n', stderr);     // 输出换行符
	exit(1);                 // 直接终止进程
}
