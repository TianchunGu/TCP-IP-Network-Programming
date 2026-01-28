/*
 * 这是一个 TCP 回显（echo）服务器示例程序：
 * 1) 从命令行读取监听端口；
 * 2) 创建 TCP 监听套接字（socket）；
 * 3) 绑定本机地址与端口（bind）；
 * 4) 开始监听（listen）；
 * 5) 循环接收客户端连接（accept），并为每个客户端提供“按行回显”服务；
 * 6) 处理完指定数量（这里是 5 个）客户端后退出。
 *
 * 典型 TCP 服务器流程（顺序非常重要）：
 * socket() -> bind() -> listen() -> accept() -> read/write -> close()
 */

#include <stdio.h>      // 标准输入输出：printf/puts/fputs/fgets/FILE/fdopen/fclose/feof/stderr
#include <stdlib.h>     // 通用工具：exit/atoi
#include <string.h>     // 字符串/内存操作：memset
#include <unistd.h>     // POSIX：close 等（文件描述符相关）
#include <arpa/inet.h>  // 网络相关：htonl/htons 等字节序转换
#include <sys/socket.h> // socket/bind/listen/accept 以及套接字相关数据结构和常量

#define BUF_SIZE 1024   // 接收/发送缓冲区大小（按行读取时的最大行长度上限）
void error_handling(char *message); // 错误处理函数：输出错误信息并退出

int main(int argc, char* argv[])
{
    int serv_sock, clnt_sock;
    /*
     * serv_sock：服务器监听套接字（listen socket）
     *   - 用来“等待”客户端连接请求，不直接负责和某个具体客户端收发数据
     *
     * clnt_sock：客户端连接套接字（connected socket）
     *   - accept() 成功后返回，用来与某个客户端进行实际数据收发
     *
     * 关键理解：
     * - 服务器端通常会同时存在两类 socket：
     *   1) 监听 socket（serv_sock）
     *   2) 已连接 socket（clnt_sock），每 accept 一个客户端就得到一个新的 clnt_sock
     */

    struct sockaddr_in serv_addr, clnt_addr;
    /*
     * sockaddr_in：IPv4 地址结构体
     * serv_addr：服务器要绑定（bind）的本地地址（IP + 端口）
     * clnt_addr：accept 时用于接收客户端地址信息（对方 IP + 端口）
     *
     * sockaddr_in 内部关键字段：
     * - sin_family：地址族（AF_INET 表示 IPv4）
     * - sin_addr.s_addr：IPv4 地址（32位，网络字节序）
     * - sin_port：端口（16位，网络字节序）
     */

    socklen_t clnt_addr_sz;
    /*
     * clnt_addr_sz：地址结构体大小变量
     * - accept() 的第三个参数是“输入输出参数”：
     *   调用前：告诉内核 clnt_addr 缓冲区的大小
     *   调用后：内核会写入实际地址长度（通常仍是 sizeof(sockaddr_in)）
     * - socklen_t 是专门用于此类长度字段的类型
     */

    char buf[BUF_SIZE]; // 通信缓冲区：用于保存从客户端读到的一行数据，并回显给客户端

    /*
     * 参数检查：
     * 本程序只需要一个参数：端口号
     * argv[0]：程序名
     * argv[1]：端口字符串，如 "8080"
     */
    if(argc != 2)
    {
        printf("Usage: %s <port>\n", argv[0]);
        exit(1); // 参数不正确直接退出
    }

    /*
     * socket()：创建一个套接字
     * PF_INET：协议族为 IPv4（Protocol Family）
     * SOCK_STREAM：字节流服务，面向连接 -> 对应 TCP
     * 0：让系统自动选择协议（对于 PF_INET + SOCK_STREAM 通常就是 TCP）
     *
     * 返回值：
     * - 成功：非负整数，表示文件描述符（FD）
     * - 失败：-1
     */
    serv_sock = socket(PF_INET, SOCK_STREAM, 0);
    if(serv_sock == -1)
        error_handling("socket() error");

    /*
     * memset：将 serv_addr 清零，避免结构体里出现“脏数据”
     * - sockaddr_in 里可能包含 padding 字段，若不清零在某些系统上可能导致意外问题
     */
    memset(&serv_addr, 0, sizeof(serv_addr));

    /*
     * sin_family：地址族
     * AF_INET：IPv4
     * 注意：socket() 里用 PF_INET，结构体这里用 AF_INET，是常见搭配
     */
    serv_addr.sin_family = AF_INET;

    /*
     * serv_addr.sin_addr.s_addr：服务器要绑定的“本地 IP”
     *
     * INADDR_ANY 的含义：
     * - 绑定到“本机所有网卡/所有本地 IP”（0.0.0.0）
     * - 也就是：只要数据包发到本机任何一个 IP + 指定端口，都能被该 socket 接收
     *
     * 为什么要 htonl(INADDR_ANY)：
     * - 网络协议规定多字节整数以“网络字节序（大端序）”传输/存储
     * - htonl：host to network long（把主机字节序 32位 转为网络字节序）
     * - INADDR_ANY 是一个 32 位值（0），写入 s_addr 前建议转换为网络序
     */
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    /*
     * serv_addr.sin_port：服务器监听端口
     * atoi(argv[1])：把端口字符串转为整数（如 "8080" -> 8080）
     * htons：host to network short（把主机字节序 16位 转为网络字节序）
     */
    serv_addr.sin_port = htons(atoi(argv[1]));

    /*
     * bind()：把“套接字”与“本地 IP:端口”绑定
     * - 对服务器来说非常关键：没有 bind，就没有“固定端口”对外提供服务
     *
     * 参数：
     * serv_sock：要绑定的 socket
     * (struct sockaddr*)&serv_addr：地址结构体指针（sockaddr_in 转 sockaddr）
     * sizeof(serv_addr)：地址结构体长度
     *
     * 失败常见原因：
     * - 端口已被占用（Address already in use）
     * - 没有权限绑定低端口（<1024 通常需要管理员权限）
     * - 地址不合法等
     */
    if(bind(serv_sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == -1)
        error_handling("bind() error");

    /*
     * listen()：将 socket 变为“监听 socket”
     * - 告诉内核：此 socket 用来接受连接请求（进入被动等待状态）
     *
     * 第二个参数 backlog：
     * - 指定“连接请求队列”的最大长度（排队等待 accept 的连接数上限）
     * - 这里设置为 5：表示允许最多 5 个连接请求排队
     *
     * 注意：
     * - backlog 并不等同于“最多只能连接 5 个客户端”，它是“等待 accept 的排队长度”
     * - 实际系统还会受内核参数影响
     */
    if(listen(serv_sock, 5) == -1)
        error_handling("listen() error");

    /*
     * 准备 accept：
     * clnt_addr_sz 初始化为 clnt_addr 缓冲区大小
     * accept 会把客户端地址信息写进 clnt_addr，同时可能更新 clnt_addr_sz
     */
    clnt_addr_sz = sizeof(clnt_addr);

    /*
     * 这里 for 循环处理 5 次 accept：
     * - 意味着服务器最多服务 5 个客户端连接（顺序处理，不是并发）
     * - 每次 accept 得到一个新连接 clnt_sock，随后进入回显循环
     *
     * 教学提示：
     * - 真实服务器通常会无限循环 accept（while(1)）
     * - 并且会用多进程/多线程/IO多路复用来实现并发处理多个客户端
     */
    for(int i = 0; i < 5; i++)
    {
        /*
         * accept()：从监听 socket 上取出一个已完成三次握手的连接
         *
         * 关键原理：
         * - 客户端 connect() 发起连接请求
         * - 内核完成 TCP 三次握手后，将连接放入“已完成连接队列”
         * - accept() 会从队列中取出一个连接，并返回一个新的“已连接 socket”（clnt_sock）
         *
         * 参数：
         * serv_sock：监听 socket
         * (struct sockaddr*)&clnt_addr：用于输出客户端地址信息
         * &clnt_addr_sz：输入输出参数，表示 clnt_addr 缓冲区大小/实际写入大小
         *
         * 返回值：
         * - 成功：新的已连接 socket FD（clnt_sock）
         * - 失败：-1
         *
         * 注意：accept() 默认是阻塞的
         * - 如果没有客户端来连接，程序会卡在这里等待
         */
        clnt_sock = accept(serv_sock, (struct sockaddr*)&clnt_addr, 
            &clnt_addr_sz);
        if(clnt_sock == -1)
            error_handling("accept() error");
        else
            puts("New Client connected...");

        /*
         * fdopen()：把 socket 的“文件描述符 FD”转换为“标准 I/O 流 FILE*”
         *
         * 为什么要这样做？
         * - 使用 FILE* 可以方便地用 fgets/fputs 做“按行”读写
         * - 对教学示例很友好
         *
         * 模式说明：
         * - "r"：读（从客户端读数据）
         * - "w"：写（向客户端写数据）
         *
         * 重要提醒（教学重点）：
         * - 标准 I/O（stdio）是“带缓冲”的：
         *   * 写入时通常先写到用户态缓冲区，缓冲满或 fflush/fclose 才真正发送
         *   * 读取时也可能先把一大块数据读入缓冲区，再逐步提供给 fgets
         * - 因此交互式网络程序通常需要 fflush() 来确保及时发送
         *
         * 另外一个潜在点：
         * - 对同一个 FD 调用两次 fdopen（一个 "r" 一个 "w"）在示例中常见
         * - 但更严谨的做法通常是：
         *   1) 用 dup() 复制一份 FD 再分别 fdopen
         *   或 2) 用单个 "r+" 流并小心处理读写切换与缓冲
         * - 本题要求不改代码，因此这里只通过注释说明注意事项
         */
        FILE* readfp = fdopen(clnt_sock, "r");
        FILE* writefp = fdopen(clnt_sock, "w");

        /*
         * feof(readfp)：判断是否到达文件结束（EOF）
         * 在网络 socket 的场景里：
         * - 当对端（客户端）正常关闭连接（close/shutdown 发送 FIN）后
         * - read 读到 0 字节，stdio 才会设置 EOF 标志
         *
         * 教学重点：while(!feof(fp)) 的常见陷阱
         * - feof 只有在“读操作失败/读到 EOF 之后”才会变为真
         * - 也就是说，通常需要先尝试读，再判断是否读到了 EOF
         * - 这段代码属于经典教学示例写法，但在工程中更常见的写法是：
         *   while(fgets(...) != NULL) { ... }
         * - 这里不改代码，只解释其行为：
         *   当客户端断开时，fgets 会返回 NULL，随后 feof 变为真，循环结束
         */
        while(!feof(readfp))
        {
            /*
             * fgets：从 readfp（socket 读流）读取一行
             * - 最多读 BUF_SIZE-1 个字符，保证 buf 以 '\0' 结尾
             * - 若读到 '\n' 会把 '\n' 一并读入 buf
             * - 假设客户端发送的是“文本行协议”（按行发送，带换行）
             *
             * 注意：
             * - 如果客户端发送的数据不包含 '\n'，fgets 可能读满缓冲或等待更多数据
             * - TCP 是“字节流”，不保留消息边界，按行只是应用层协议约定
             */
            fgets(buf, BUF_SIZE, readfp);

            /*
             * fputs：把刚读到的一行原样写回客户端（回显）
             * - 这就是 echo 服务器的核心：读什么回什么
             */
            fputs(buf, writefp);

            /*
             * fflush：强制刷新 writefp 的输出缓冲区
             * - 确保数据立刻从用户态缓冲发送到内核 socket 缓冲，从而尽快到达客户端
             *
             * 若不 fflush 可能出现：
             * - 客户端迟迟收不到回显（因为数据还在 stdio 的缓冲区里）
             * - 直到缓冲区满或 fclose 才一次性发出，影响交互体验
             */
            fflush(writefp);
            // 标准I/O函数为了提高性能，内部提供额外的缓冲。
            // 若不调用fflush函数则无法保证立即将数据传输到客户端
        }

        /*
         * 关闭流：
         * fclose 会刷新缓冲并释放 FILE* 资源，并且通常会关闭其底层 FD（close）
         *
         * 教学提醒：
         * - 若同一个 clnt_sock 被两个 FILE* 包装（readfp/writefp）
         *   分别 fclose 可能触发“重复 close 同一个 FD”的风险
         * - 在很多示例/实现中仍可能“看起来能跑”，但不属于最稳妥写法
         * - 规范工程写法会避免这种风险（例如 dup 后分别 fdopen）
         * - 本题不允许改动代码，因此这里只说明原理与潜在问题
         */
        fclose(readfp);
        fclose(writefp);
    }

    /*
     * 关闭监听 socket：
     * - 表示服务器不再接受新的连接
     * - serv_sock 只是监听用的，不影响已建立连接（但此时已处理完 5 个客户端）
     *
     * close 是 POSIX 的文件描述符关闭函数
     */
    close(serv_sock);
    return 0;
}

/*
 * error_handling：简单的错误处理函数
 * - 将错误信息输出到标准错误 stderr（便于和正常输出区分）
 * - 输出换行
 * - 以非 0 状态码退出程序（表示异常结束）
 *
 * 教学提示：
 * - 这里打印的是自定义字符串，并没有输出 errno 具体原因
 * - 更完整的错误处理可用 perror 或 strerror(errno)，但本题不改代码
 */
void error_handling(char* message)
{
    fputs(message, stderr);
    fputc('\n', stderr);
    exit(1);
}
