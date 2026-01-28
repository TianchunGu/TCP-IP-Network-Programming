#include <stdio.h>      // 标准I/O：printf, puts, fputs, fgets, FILE, fdopen, fclose, stdout, stdin, stderr
#include <stdlib.h>     // exit, atoi
#include <string.h>     // memset, strcmp
#include <unistd.h>     // POSIX：close 等（本程序未显式 close，但 socket FD 属于文件描述符体系）
#include <arpa/inet.h>  // inet_addr, htons：IP字符串转换、网络字节序转换
#include <sys/socket.h> // socket, connect 以及 socket 相关结构/常量

#define BUF_SIZE 1024   // 缓冲区大小：用于保存用户输入与服务器返回的数据
void error_handling(char *message); // 统一错误处理函数声明

int main(int argc, char* argv[])
{
    int serv_sock;
    /*
     * serv_sock：客户端套接字（socket）文件描述符
     * - socket() 成功后返回一个非负整数（FD）
     * - connect() 成功后，该 FD 对应的 socket 进入“已连接”状态
     * - 后续即可通过该 FD 与服务器进行数据收发
     */

    struct sockaddr_in serv_addr;
    /*
     * serv_addr：服务器地址结构体（IPv4）
     * - 需要填入：地址族 AF_INET、服务器 IP、服务器端口（网络字节序）
     * - connect() 会使用此结构体确定要连接的服务器位置
     */

    char message[BUF_SIZE];
    /*
     * message：应用层缓冲区
     * - 用来存放用户输入的一行文本
     * - 也用来存放服务器返回（回显/响应）的一行文本
     */

    /*
     * 参数检查：
     * 该程序需要两个参数：
     * argv[1]：服务器 IP（点分十进制字符串，如 "127.0.0.1"）
     * argv[2]：服务器端口（字符串，如 "8080"）
     *
     * argc 应为 3（argv[0] 是程序名）
     */
    if(argc != 3)
    {
        printf("Usage: %s <IP> <port>\n", argv[0]);
        exit(1); // 参数错误，无法继续运行
    }

    /* -------------------- 第一部分：创建并连接 TCP socket -------------------- */

    /*
     * socket(PF_INET, SOCK_STREAM, 0)
     * - PF_INET：IPv4 协议族
     * - SOCK_STREAM：面向连接的字节流服务 -> TCP
     * - 0：自动选择协议（对 PF_INET + SOCK_STREAM 通常就是 TCP）
     *
     * 返回值：
     * - 成功：非负整数 FD
     * - 失败：-1
     */
    serv_sock = socket(PF_INET, SOCK_STREAM, 0);
    if(serv_sock == -1)
        error_handling("socket() error");

    /*
     * 初始化服务器地址结构体：
     * memset 清零避免未初始化字段造成不可预期行为
     */
    memset(&serv_addr, 0, sizeof(serv_addr));

    /*
     * 设置地址族：IPv4
     */
    serv_addr.sin_family = AF_INET;

    /*
     * inet_addr(argv[1])：把点分十进制 IP 字符串转为 32 位网络字节序 IPv4 地址
     * - argv[1] 例如 "192.168.1.10"
     * - 转换结果直接赋给 sin_addr.s_addr（该字段要求网络字节序）
     *
     * 教学补充：
     * - inet_addr 属于较老接口，现代常用 inet_pton
     * - 但本题要求不改代码，这里只解释原理
     */
    serv_addr.sin_addr.s_addr = inet_addr(argv[1]);

    /*
     * 端口设置：
     * atoi(argv[2])：端口字符串 -> 整数（主机字节序）
     * htons：host to network short，把 16 位端口转为网络字节序（大端序）
     */
    serv_addr.sin_port = htons(atoi(argv[2]));

    /*
     * connect：发起 TCP 连接（三次握手）
     * 参数：
     * - serv_sock：客户端 socket FD
     * - (struct sockaddr*)&serv_addr：服务器地址（通用 sockaddr 指针）
     * - sizeof(serv_addr)：地址结构长度
     *
     * 返回值：
     * - 成功：0
     * - 失败：-1（例如服务器未启动/端口未监听/网络不可达等）
     */
    if(connect(serv_sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == -1)
        error_handling("connect() error");
    else
        puts("Connected..."); // 成功建立连接

    /* -------------------- 第二部分：把 socket FD 转换为标准I/O流 -------------------- */

    /*
     * fdopen：把“文件描述符 FD”包装成“标准 I/O 流 FILE*”
     *
     * 为什么这样做（教学重点）：
     * - 使用 FILE* 可以用 fgets/fputs 方便地按行读写文本
     * - 但 stdio 有缓冲机制：写入可能先进入用户态缓冲区，需 fflush 才能立即发送
     *
     * 重要提醒（更深入）：
     * - 这里对同一个 FD（serv_sock）调用了两次 fdopen：
     *   得到 readfp("r") 与 writefp("w")
     * - 在示例代码中常见，但严格工程实践中要小心：
     *   * 两个 FILE* 共享同一个底层 FD
     *   * fclose(readfp)/fclose(writefp) 都可能关闭同一个 FD，存在“重复关闭”的潜在风险
     *   * 更稳妥的方式常见做法是：dup 一份 FD 再分别 fdopen，或用单个 "r+" 流
     * - 本题要求不改代码，因此仅在注释中提示
     */
    FILE* readfp = fdopen(serv_sock, "r");  // 从服务器读数据的流
    FILE* writefp = fdopen(serv_sock, "w"); // 向服务器写数据的流

    /* -------------------- 第三部分：交互循环（发送 -> 接收 -> 打印） -------------------- */

    while(1)
    {
        /*
         * 提示用户输入
         * fputs 输出到 stdout（标准输出）
         */
        fputs("Input message(q/Q to quit): ", stdout);

        /*
         * 从 stdin（标准输入）读取一行到 message：
         * - 最多读取 BUF_SIZE-1 个字符
         * - 会保留换行符 '\n'（如果读到了）
         * - 并在末尾添加 '\0' 形成 C 字符串
         */
        fgets(message, BUF_SIZE, stdin);

        /*
         * 判断是否退出：
         * 因为 fgets 会把 '\n' 读进来，所以比较的是 "q\n" / "Q\n"
         */
        if(!strcmp(message, "q\n") || !strcmp(message, "Q\n"))
            break;

        /*
         * 把用户输入发送到服务器：
         * fputs 把 message 写到 writefp（写流）
         *
         * 教学重点：stdio 缓冲
         * - fputs 并不保证立刻把数据发送到网络
         * - 数据可能先进入 writefp 的用户态缓冲区
         */
        fputs(message, writefp);

        /*
         * fflush(writefp)：强制刷新 writefp 的输出缓冲区
         * - 让数据立即从用户态缓冲写入内核 socket 缓冲，从而尽快发到服务器
         * - 交互式网络程序一般必须这样做，否则可能出现“服务器收不到/延迟很久才收到”
         */
        fflush(writefp);

        /*
         * 从服务器读取一行响应：
         * - 假设服务器也按行发送数据（包含 '\n'）
         * - fgets 会一直读到 '\n'、或读满 BUF_SIZE-1、或遇到 EOF
         *
         * 教学提示：
         * - TCP 是字节流，不保证“消息边界”
         * - 这里按行读写是一种应用层协议约定：双方都用 '\n' 作为一条消息的结束标记
         */
        fgets(message, BUF_SIZE, readfp);

        /*
         * 打印服务器返回的消息：
         * message 本身可能已包含 '\n'，这里 printf 又加 "\n"
         * 可能导致显示上出现空行（双换行）。这是展示效果问题，不改代码。
         */
        printf("Message from server: %s\n", message);
    }

    /* -------------------- 第四部分：关闭资源 -------------------- */

    /*
     * 关闭 FILE*：
     * - fclose 会刷新缓冲并释放 FILE* 资源
     * - 通常还会关闭其底层 FD（相当于 close(serv_sock)）
     *
     * 教学提醒：
     * - 因为 readfp/writefp 共享同一个底层 FD，分别 fclose 可能存在重复关闭的风险
     * - 但示例代码常用此写法，本题要求不改代码，因此只解释
     */
    fclose(writefp);
    fclose(readfp);

    return 0;
}

/*
 * error_handling：统一错误处理函数
 * - 输出错误信息到 stderr
 * - 输出换行
 * - 退出程序（非 0 表示异常）
 */
void error_handling(char *message)
{
	fputs(message, stderr);
	fputc('\n', stderr);
	exit(1);
}
