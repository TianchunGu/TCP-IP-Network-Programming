#include <stdio.h>      // printf, sprintf, fgets, fputs, stdin, stdout, stderr
#include <stdlib.h>     // exit, atoi
#include <unistd.h>     // close, read, write（POSIX I/O）
#include <string.h>     // memset, strlen, strcmp
#include <arpa/inet.h>  // inet_addr, htons：IP字符串转换、端口字节序转换
#include <sys/socket.h> // socket, connect 等套接字接口
#include <pthread.h>    // pthread_create, pthread_join, pthread_t：POSIX 线程库

#define NAME_SIZE 20    // 用户名最大长度（含括号等）；用于消息前缀显示
#define BUF_SIZE 100    // 单次输入/接收的消息缓冲区大小

/*
 * 线程函数声明：
 * - snd_msg：发送线程（从键盘读输入 -> 发送给服务器）
 * - rcv_msg：接收线程（从服务器读数据 -> 打印到屏幕）
 *
 * 这种“一个线程负责发送 + 一个线程负责接收”的结构常用于聊天客户端，
 * 可以实现：一边输入发送，一边实时接收别人消息（互不阻塞）。
 */
void* rcv_msg(void* arg);
void* snd_msg(void* arg);
void error_handling(char* message);

/*
 * 全局变量（所有线程都可访问）：
 *
 * name：用户名前缀，默认 "[DEFAULT]"
 * - 程序启动后会用 argv[3] 重新设置为 "[xxx]" 格式
 *
 * msg：用于保存从 stdin 读到的输入
 * - 注意：它是全局变量，snd_msg 线程会写它
 * - rcv_msg 线程没有用 msg，因此不会与接收线程产生直接读写冲突
 * - 但如果未来扩展程序让多个线程同时用 msg，就需要考虑线程安全
 */
char name[NAME_SIZE] = "[DEFAULT]";
char msg[BUF_SIZE];

int main(int argc, char* argv[])
{
    int sock;
    struct sockaddr_in serv_addr;
    pthread_t id_snd, id_rcv;
    void* thread_rt; // 用于接收 pthread_join 的线程返回值（本例中不使用其内容）

    /*
     * 参数检查：需要 3 个参数
     * argv[1]：服务器IP
     * argv[2]：服务器端口
     * argv[3]：用户名
     */
    if(argc != 4)
    {
        printf("Usage: %s <ip> <port> <name>\n", argv[0]);
        exit(1);
    }

    /*
     * sprintf(name, "[%s]", argv[3])
     * - 将用户名包装成形如 "[Alice]" 的前缀
     * - 后续发送消息时，会把该前缀拼到消息前面
     *
     * 教学提示（不改代码，仅说明）：
     * - 如果 argv[3] 很长，可能会超过 NAME_SIZE 导致溢出风险
     * - 更安全的做法通常用 snprintf 并做长度检查
     */
    sprintf(name, "[%s]", argv[3]);

    /*
     * 创建 TCP socket：
     * PF_INET：IPv4
     * SOCK_STREAM：TCP（面向连接的字节流）
     * 0：自动选择协议（通常为 TCP）
     *
     * 返回值 sock 是文件描述符（FD），之后 connect/read/write/close 都用它
     */
    sock = socket(PF_INET, SOCK_STREAM, 0);

    /*
     * 初始化服务器地址结构体 serv_addr
     */
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;                  // IPv4
    serv_addr.sin_addr.s_addr = inet_addr(argv[1]);  // 服务器 IP（点分十进制字符串 -> 32位网络序）
    serv_addr.sin_port = htons(atoi(argv[2]));       // 端口：字符串->整数->网络字节序

    /*
     * connect：连接服务器（TCP 三次握手）
     * - 成功返回 0，失败返回 -1
     */
    if(connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == -1)
        error_handling("connect() error");

    /*
     * 创建两个线程：
     *
     * pthread_create(&id_snd, NULL, snd_msg, (void*)&sock);
     * - 发送线程：从键盘读入文本并发送到服务器
     *
     * pthread_create(&id_rcv, NULL, rcv_msg, (void*)&sock);
     * - 接收线程：从服务器读消息并输出到屏幕
     *
     * 第四个参数传入的是 sock 的地址：
     * - 线程启动后会把 arg 强转回 int* 再取值
     * - 这样两个线程就共享同一个 socket 连接
     *
     * 教学重点：为什么需要两个线程？
     * - 如果只用一个线程：read 可能阻塞，导致无法同时读取键盘输入
     * - 两线程模型可以让“接收”与“发送”互不阻塞，体验更像真实聊天
     */
    pthread_create(&id_snd, NULL, snd_msg, (void*)&sock);
    pthread_create(&id_rcv, NULL, rcv_msg, (void*)&sock);

    /*
     * pthread_join：等待线程结束（阻塞）
     * - 主线程会在这里等待发送线程与接收线程都结束后再继续
     */
    pthread_join(id_snd, &thread_rt);
    pthread_join(id_rcv, &thread_rt);

    /*
     * 关闭 socket
     * - 理论上：如果线程里已经 close(sock) 并 exit(0)，这里可能执行不到
     * - 但写上 close 是规范习惯（确保资源释放）
     */
    close(sock);

    return 0;
}

void* snd_msg(void* arg)
{
    /*
     * 线程参数 arg 指向 main 中的 sock 变量（地址传递）
     * 这里把它解释为 int* 并取出 socket FD 值
     */
    int sock = *((int *)arg);

    /*
     * name_msg：最终要发送的消息缓冲区
     * - 包含 name 前缀 + 空格 + 用户输入 msg
     * - 因此长度上限设为 BUF_SIZE + NAME_SIZE
     */
    char name_msg[BUF_SIZE + NAME_SIZE];

    while(1)
    {
        /*
         * fgets(msg, BUF_SIZE, stdin)：
         * - 从标准输入读取一行到全局变量 msg
         * - 最多读取 BUF_SIZE-1 个字符，并以 '\0' 结尾
         * - 如果读到换行符 '\n'，会把 '\n' 也存入 msg
         */
        fgets(msg, BUF_SIZE, stdin);

        /*
         * 退出条件：用户输入 q 或 Q（注意包含换行）
         * - 因为 fgets 会保留 '\n'，所以比较的是 "q\n"/"Q\n"
         */
        if(!strcmp(msg, "q\n") || !strcmp(msg, "Q\n"))
        {
            /*
             * close(sock)：关闭 socket
             * - 关闭后对端（服务器）通常会检测到连接断开（read 返回 0 或异常）
             *
             * exit(0)：直接结束整个进程
             * 教学重点：
             * - exit 会终止整个进程，因此其它线程也会被结束
             * - 所以这里不需要显式通知 rcv_msg 线程停止（进程直接没了）
             */
            close(sock);
            exit(0);
        }

        /*
         * sprintf(name_msg, "%s %s", name, msg)
         * - 把用户名和输入消息拼起来，例如：
         *   "[Alice] hello\n"
         *
         * 教学提示（不改代码，仅说明）：
         * - 如果 name + msg 总长度超过 name_msg 缓冲区，可能溢出
         * - 更安全通常用 snprintf 并做长度检查
         */
        sprintf(name_msg, "%s %s", name, msg);

        /*
         * write(sock, name_msg, strlen(name_msg))
         * - 向服务器发送拼接后的消息
         *
         * 教学重点：TCP 是“字节流”
         * - write 发送的是一段字节序列，不保留“消息边界”
         * - 服务端通常需要用应用层协议来分隔消息（这里依赖 '\n' 作为行结束）
         *
         * 教学提示：
         * - write 在某些情况下可能只发送部分数据（返回值 < 请求长度）
         *   严谨实现需要处理“部分写”，但聊天示例通常简化
         */
        write(sock, name_msg, strlen(name_msg));
    }

    /*
     * 实际上因为 while(1) 永不退出（除非 exit），这里通常不会执行到
     * 仍然返回 NULL 以满足线程函数返回类型要求
     */
    return NULL;
}

void* rcv_msg(void* arg)
{
    /*
     * 从 arg 取出 socket FD
     */
    int sock = *((int*)arg);

    /*
     * name_msg：接收缓冲区
     * - 最大接收 BUF_SIZE + NAME_SIZE - 1 字节，
     *   留一个位置给手动添加的字符串结束符 '\0'
     */
    char name_msg[BUF_SIZE + NAME_SIZE];
    int str_len;

    while(1)
    {
        /*
         * read(sock, name_msg, BUF_SIZE + NAME_SIZE - 1)
         * - 从服务器读取数据
         *
         * read 返回值含义（教学重点）：
         * - >0：实际读到的字节数
         * -  0：对端关闭连接（EOF）
         * - -1：出错（例如连接被重置等）
         *
         * 本代码只处理 -1 的情况：直接返回 (void*)-1
         * 教学提示：
         * - 若服务器正常关闭，read 可能返回 0；本代码未处理 0，会导致死循环或输出异常
         * - 真实聊天客户端一般会在 str_len == 0 时退出接收线程
         */
        str_len = read(sock, name_msg, BUF_SIZE + NAME_SIZE - 1);
        if(str_len == -1)
            return (void*)-1;

        /*
         * 由于 read 读取的是“原始字节”，不会自动补 '\0'
         * 但 fputs 需要 C 字符串（以 '\0' 结尾）
         * 因此手动在末尾补 '\0'，把接收到的字节变成可打印字符串
         */
        name_msg[str_len] = 0;

        /*
         * 输出到标准输出：
         * - 服务器发来的数据通常包含 '\n'，因此会换行显示
         * - fputs 不会自动加换行，显示效果完全由消息内容决定
         */
        fputs(name_msg, stdout);
    }

    return NULL;
}

void error_handling(char* msg)
{
    /*
     * 输出错误信息到标准错误 stderr，并退出程序
     */
    fputs(msg, stderr);
    fputc('\n', stderr);
    exit(1);
}
