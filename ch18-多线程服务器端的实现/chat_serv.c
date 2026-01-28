#include <stdio.h>      // printf, fputs, stderr
#include <stdlib.h>     // exit, atoi
#include <unistd.h>     // read, write, close（POSIX I/O）
#include <string.h>     // memset
#include <arpa/inet.h>  // htonl, htons, inet_ntoa（网络字节序与地址转换）
#include <sys/socket.h> // socket, bind, listen, accept（套接字系统调用）
#include <netinet/in.h> // sockaddr_in 等（与 arpa/inet.h 配合使用）
#include <pthread.h>    // pthread_create, pthread_detach, pthread_mutex_*（线程与互斥锁）

#define BUF_SIZE 100    // 每次从客户端读消息的缓冲区大小（一次最多读 100 字节）
#define MAX_CLNT 256    // 允许同时连接的最大客户端数量（客户端 socket 数组容量）

/*
 * 线程函数：每接入一个客户端就创建一个线程来处理该客户端
 * - handle_clnt：负责从某个客户端循环读取消息，并广播给所有客户端
 * 辅助函数：
 * - send_msg：把某条消息写给所有已连接客户端（广播）
 * - error_handling：错误处理，打印并退出
 */
void* handle_clnt(void* arg);
void send_msg(char *msg, int len);
void error_handling(char* message);

/* -------------------- 全局共享数据（多线程共享，需要互斥保护） -------------------- */

pthread_mutex_t mutex;          // 互斥锁：保护 clnt_socks 与 clnt_cnt 的并发访问
int clnt_socks[MAX_CLNT];       // 保存所有已连接客户端的 socket FD（文件描述符）
int clnt_cnt = 0;               // 当前已连接客户端数量

int main(int argc, char* argv[])
{
    int serv_sock, clnt_sock;
    struct sockaddr_in serv_addr, clnt_addr;
    socklen_t clnt_add_sz;
    pthread_t t_id;

    /*
     * 参数检查：
     * - 服务器只需要一个参数：监听端口
     * argv[0]：程序名
     * argv[1]：端口字符串，例如 "8080"
     */
    if(argc != 2)
    {
        printf("Usage: %s <port>\n", argv[0]);
        exit(1);
    }

    /*
     * 初始化互斥锁：
     * - mutex 用于保护共享资源 clnt_socks[] 与 clnt_cnt
     * - 因为多个客户端线程会同时读取/修改这些全局变量
     */
    pthread_mutex_init(&mutex, NULL);

    /* -------------------- 第一部分：建立 TCP 监听 socket -------------------- */

    /*
     * socket(PF_INET, SOCK_STREAM, 0)
     * - PF_INET：IPv4
     * - SOCK_STREAM：TCP（面向连接的字节流）
     * - 0：自动选择协议（通常为 TCP）
     */
    serv_sock = socket(PF_INET, SOCK_STREAM, 0);
    if(serv_sock == -1)
        error_handling("socket() error");

    /*
     * 填充服务器地址结构体：
     * memset 清零避免未初始化字段影响 bind
     */
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;               // IPv4
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);// 绑定 0.0.0.0（本机所有 IP）
    serv_addr.sin_port = htons(atoi(argv[1]));    // 端口（网络字节序）

    /*
     * bind：绑定本地 IP:端口
     * 常见失败原因：
     * - 端口被占用
     * - 权限不足（绑定 < 1024 端口）
     */
    if(bind(serv_sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == -1)
        error_handling("bind() error");

    /*
     * listen：开始监听
     * backlog=5：连接请求等待队列长度上限
     */
    if(listen(serv_sock, 5) == -1)
        error_handling("listen() error");

    /* -------------------- 第二部分：主线程循环 accept 新连接 -------------------- */

    while(1)
    {
        /*
         * accept：
         * - 阻塞等待新客户端连接
         * - 成功后返回一个新的已连接 socket（clnt_sock）
         * - clnt_addr 保存对方地址信息（IP/port）
         */
        clnt_add_sz = sizeof(clnt_addr);
        clnt_sock = accept(serv_sock, (struct sockaddr*)&clnt_addr, &clnt_add_sz);
        if(clnt_sock == -1)
            error_handling("accept() error");

        /*
         * 新客户端连接成功后，需要把该客户端 socket 加入全局数组 clnt_socks[]
         * 这是共享资源，必须加锁保护：
         * - 防止其它线程正在遍历/写入 clnt_socks 时出现数据竞争
         */
        pthread_mutex_lock(&mutex);
        clnt_socks[clnt_cnt++] = clnt_sock;
        pthread_mutex_unlock(&mutex);

        /*
         * 为该客户端创建一个独立线程处理收消息：
         * - handle_clnt 线程会不断 read 该客户端发来的数据
         * - 并调用 send_msg 广播给所有客户端
         *
         * 注意（教学重点，理解潜在风险）：
         * - 这里把 (void*)&clnt_sock 传给线程函数。
         * - clnt_sock 是 main 线程栈上的局部变量，并且会在下一次循环中被改写。
         * - 如果线程启动较晚，可能读到被改写后的值，导致线程拿错 socket FD。
         *
         * 工程上更稳妥做法通常是：
         * - 为每个客户端单独分配一块内存存放 fd（malloc），或
         * - 使用全局/堆结构保存并传入指针，确保生命周期足够
         * 但题目要求不改代码，因此这里只在注释中说明。
         */
        pthread_create(&t_id, NULL, handle_clnt, (void*)&clnt_sock);

        /*
         * pthread_detach：
         * - 将线程设置为“分离态”（detached）
         * - 线程结束后资源会自动回收，无需 pthread_join
         * - 适合服务器这种“不断产生短/长生命周期工作线程”的场景
         */
        pthread_detach(t_id);

        /*
         * 打印新连接客户端的 IP：
         * inet_ntoa 将网络地址转换为点分十进制字符串（如 "192.168.1.5"）
         */
        printf("Connected client IP: %s\n", inet_ntoa(clnt_addr.sin_addr));
    }

    /*
     * 主循环理论上不会退出，这里 close(serv_sock) 属于善后代码
     */
    close(serv_sock);

    return 0;
}

void* handle_clnt(void* arg)
{
    /*
     * 线程入口：
     * - arg 传入的是 socket FD 的地址（int*）
     * - 取出该客户端的 socket FD
     */
    int clnt_sock = *((int*)arg);

    int str_len = 0;
    char msg[BUF_SIZE];

    /*
     * 循环读取该客户端发送的数据，并广播：
     *
     * read 返回值含义（教学重点）：
     * - >0：读到的字节数
     * -  0：对端关闭连接（EOF）
     * - -1：出错（本代码未处理 -1，实际中应处理并关闭连接）
     *
     * 本循环条件：read(...) != 0
     * - 当客户端正常断开时，read 返回 0，循环结束
     */
    while((str_len = read(clnt_sock, msg, sizeof(msg))) != 0)
        send_msg(msg, str_len);

    /*
     * 客户端断开后，需要从全局客户端数组 clnt_socks[] 中移除该 socket：
     * 这也是共享资源，必须加锁保护。
     */
    pthread_mutex_lock(&mutex);

    /*
     * 在数组中找到对应的 clnt_sock，并将其删除：
     * - 删除方式：用后面的元素覆盖当前元素（整体左移）
     * - 这样可以保持数组连续，方便遍历广播
     */
    for(int i = 0; i < clnt_cnt; i++)
    {
        if(clnt_socks[i] == clnt_sock)
        {
            /*
             * 将 i 后面的元素全部向前移动一位：
             * - i++ < clnt_cnt - 1 控制移动范围
             * - clnt_socks[i] = clnt_socks[i + 1] 完成覆盖
             */
            while(i++ < clnt_cnt - 1)
                clnt_socks[i] = clnt_socks[i + 1];
            break;
        }
    }

    /*
     * 客户端数量减 1
     */
    clnt_cnt--;

    pthread_mutex_unlock(&mutex);

    /*
     * 关闭该客户端 socket
     */
    close(clnt_sock);

    return NULL;
}

void send_msg(char* msg, int len)
{
    /*
     * 广播函数：把一条消息发送给所有已连接客户端
     *
     * 教学重点：为什么这里也要加锁？
     * - 因为同时可能有线程在 handle_clnt 中删除客户端、修改 clnt_cnt/clnt_socks
     * - 如果不加锁，遍历过程中数组可能被改变导致越界、写到无效 fd 等
     */
    pthread_mutex_lock(&mutex);

    for(int i = 0; i < clnt_cnt; i++)
    {
        /*
         * write：向每个客户端 socket 写入同样的消息
         *
         * 教学提示：
         * - write 可能出现“部分写”（返回值 < len），严谨实现需要处理
         * - 若某个客户端异常断开，write 可能失败（例如 EPIPE），本示例未处理
         */
        write(clnt_socks[i], msg, len);
    }

    pthread_mutex_unlock(&mutex);
}

void error_handling(char* message)
{
    /*
     * 简单错误处理：输出错误信息并退出
     * - 输出到 stderr（标准错误）
     * - exit(1) 表示异常结束
     */
    fputs(message, stderr);
    fputc('\n', stderr);
    exit(1);
}
