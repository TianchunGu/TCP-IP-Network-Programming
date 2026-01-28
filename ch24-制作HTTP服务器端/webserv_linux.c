#include <stdio.h>      // printf, fputs, fgets, FILE, fdopen, fopen, fclose, fflush 等标准I/O
#include <stdlib.h>     // exit, atoi
#include <unistd.h>     // close, dup（文件描述符复制）
#include <string.h>     // memset, strcpy, strcmp, strstr, strtok
#include <arpa/inet.h>  // inet_ntoa, htonl, htons, ntohs：IP/端口转换与字节序
#include <sys/socket.h> // socket, bind, listen, accept：套接字系统调用
#include <pthread.h>    // pthread_create, pthread_detach：多线程

#define BUF_SIZE 1024   // 发送文件内容时的缓冲区大小（一次最多读 1024 字节）
#define SMALL_BUF 100   // 解析请求行、拼接响应头等用的小缓冲区

/*
 * 这是一个非常简化的“多线程 HTTP 服务器”示例：
 * - 主线程：负责 accept 新连接，并为每个连接创建一个线程
 * - 工作线程 request_handler：只解析 HTTP 请求的第一行（Request-Line）
 *   仅支持 GET 方法，并把请求的文件内容作为响应发回客户端
 *
 * 教学重点：
 * 1) HTTP 的基本结构：请求行、响应行、响应头、空行、响应体
 * 2) TCP 连接 + 标准 I/O（fdopen/fputs/fgets）结合使用
 * 3) 多线程：每个连接一个线程（并发处理多个客户端）
 * 4) 该示例为了简单，省略了大量工程级健壮性处理（注释会指出）
 */

void *request_handler(void *arg);                 // 线程入口：处理一个客户端请求
void send_data(FILE *fp, char *ct, char *file_name); // 发送 200 OK + 文件内容
char *content_type(char *file);                   // 根据文件扩展名确定 MIME 类型
void send_error(FILE *fp);                        // 发送 400 Bad Request
void error_handling(char *message);               // 通用错误处理（打印并退出）

int main(int argc, char *argv[])
{
    int serv_sock, clnt_sock;
    struct sockaddr_in serv_adr, clnt_adr;
    int clnt_adr_size;
    pthread_t t_id;

    /*
     * 参数：只需要一个端口号
     * argv[1]：端口字符串，例如 "8080"
     */
    if (argc != 2)
    {
        printf("Usage : %s <port>\n", argv[0]);
        exit(1);
    }

    /* -------------------- 第一部分：创建并启动监听 socket -------------------- */

    /*
     * socket(PF_INET, SOCK_STREAM, 0)
     * - PF_INET：IPv4
     * - SOCK_STREAM：TCP
     * - 0：自动选择协议（通常为 TCP）
     */
    serv_sock = socket(PF_INET, SOCK_STREAM, 0);

    /*
     * 设置服务器地址：
     * - sin_family：IPv4
     * - sin_addr：INADDR_ANY 表示绑定本机所有 IP（0.0.0.0）
     * - sin_port：监听端口（网络字节序）
     */
    memset(&serv_adr, 0, sizeof(serv_adr));
    serv_adr.sin_family = AF_INET;
    serv_adr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_adr.sin_port = htons(atoi(argv[1]));

    /*
     * bind：绑定本地地址与端口
     */
    if (bind(serv_sock, (struct sockaddr *)&serv_adr, sizeof(serv_adr)) == -1)
        error_handling("bind() error");

    /*
     * listen：进入监听状态
     * backlog=20：连接请求等待队列长度上限
     */
    if (listen(serv_sock, 20) == -1)
        error_handling("listen() error");

    /* -------------------- 第二部分：主循环 accept 并创建线程处理 -------------------- */

    while (1)
    {
        /*
         * accept：阻塞等待新连接
         * - 返回新的已连接 socket：clnt_sock
         * - clnt_adr 保存客户端 IP/端口
         */
        clnt_adr_size = sizeof(clnt_adr);
        clnt_sock = accept(serv_sock, (struct sockaddr *)&clnt_adr, &clnt_adr_size);

        /*
         * inet_ntoa：把网络字节序的 IPv4 地址转成点分十进制字符串
         * ntohs：网络字节序 -> 主机字节序（把端口转回来用于打印）
         */
        printf("Connection Request : %s:%d\n",
               inet_ntoa(clnt_adr.sin_addr), ntohs(clnt_adr.sin_port));

        /*
         * 为每个客户端创建一个线程处理：
         * - request_handler 会解析请求并返回响应
         * - pthread_detach 让线程结束后自动回收资源（无需 join）
         *
         * 教学提示（重要，不改代码，仅说明潜在风险）：
         * - 这里把 &clnt_sock（主线程栈上的局部变量地址）传给线程
         * - 主线程会很快进入下一轮循环并改写 clnt_sock
         * - 如果新线程还没来得及读取 arg，可能读到被改写后的值，导致线程使用错误的 socket
         *   （典型的“传栈上变量地址给线程”的坑）
         * - 工程上通常做法：为每个连接 malloc 一个 int 保存 fd，再传指针，线程里用完 free
         */
        pthread_create(&t_id, NULL, request_handler, &clnt_sock);
        pthread_detach(t_id);
    }

    /*
     * 理论上主循环不会退出，这里 close 属于善后代码
     */
    close(serv_sock);
    return 0;
}

void *request_handler(void *arg)
{
    /*
     * 线程处理一个客户端请求：
     * - 读取 HTTP 请求的第一行（Request-Line）
     * - 仅支持 GET 方法
     * - 解析出文件名并发送对应文件内容
     */
    int clnt_sock = *((int *)arg);

    char req_line[SMALL_BUF];
    /*
     * req_line：存放 HTTP 请求行
     * 典型请求行格式：
     *   GET /index.html HTTP/1.1
     * 注意：这里只读第一行，不处理后续头字段
     */

    FILE *clnt_read;
    FILE *clnt_write;

    char method[10];
    char ct[15];
    char file_name[30];
    /*
     * method：保存请求方法，如 "GET"
     * ct：保存内容类型，如 "text/html"
     * file_name：保存请求的文件名，如 "index.html"
     *
     * 教学提示（不改代码，仅说明）：
     * - 这些缓冲区长度都较小，若请求行过长或文件名过长，可能溢出
     * - 工程实现需更严谨的长度检查
     */

    /*
     * fdopen：把 socket FD 包装成标准I/O流
     * - clnt_read 用于 fgets 读请求
     * - clnt_write 用于 fputs 写响应
     *
     * 这里对写端使用 fdopen(dup(clnt_sock),"w")：
     * 教学重点：为什么要 dup？
     * - 如果对同一个 FD 分别 fdopen 成读流和写流，两个 fclose 都可能关闭同一 FD
     * - 通过 dup 得到一个新的 FD 给写流使用：
     *   clnt_read 关闭原 FD，clnt_write 关闭 dup 出来的 FD，避免“双重关闭”的风险
     */
    clnt_read = fdopen(clnt_sock, "r");
    clnt_write = fdopen(dup(clnt_sock), "w");

    /*
     * 读取请求行（只读第一行）：
     * - fgets 读取到 '\n' 或读满 SMALL_BUF-1 为止
     * - HTTP 请求行通常以 "\r\n" 结尾，fgets 会把 '\n' 读入缓冲
     */
    fgets(req_line, SMALL_BUF, clnt_read);

    /*
     * 粗略判断是否像 HTTP 请求：
     * - 若请求行中不包含 "HTTP/"，认为不是 HTTP 请求，返回错误页面
     */
    if (strstr(req_line, "HTTP/") == NULL)
    {
        send_error(clnt_write);
        fclose(clnt_read);
        fclose(clnt_write);
        return; // 注意：此处返回类型为 void*，但直接 return 在 C 中允许（等价返回 NULL）
    }

    /*
     * 解析请求行：
     * 示例请求行：GET /index.html HTTP/1.1
     *
     * strtok(req_line, " /") 的分隔符是 空格 和 斜杠 '/'
     * 第一次 strtok 得到 method="GET"
     * 第二次 strtok 得到 file_name="index.html"（因为把 / 当分隔符吃掉了）
     *
     * 教学提示：
     * - strtok 会“修改原字符串”，把分隔符位置置为 '\0'
     * - strtok 不是线程安全函数（strtok_r 才是），但这里每个线程有自己的 req_line 缓冲，通常没问题
     */
    strcpy(method, strtok(req_line, " /"));
    strcpy(file_name, strtok(NULL, " /"));

    /*
     * 根据文件名扩展名得到 Content-Type
     * - html/htm -> text/html
     * - 其它 -> text/plain（非常简化）
     */
    strcpy(ct, content_type(file_name));

    /*
     * 仅支持 GET 方法：
     * - 如果不是 GET，则返回 400 错误
     *
     * 教学提示：
     * - HTTP 中更合适的状态码可能是 405 Method Not Allowed
     * - 但这里简化为 400 Bad Request
     */
    if (strcmp(method, "GET") != 0)
    {
        send_error(clnt_write);
        fclose(clnt_read);
        fclose(clnt_write);
        return;
    }

    /*
     * 读完请求行后就关闭读流：
     * - 这个示例并不读取剩余的请求头，也不支持持久连接（keep-alive）
     */
    fclose(clnt_read);

    /*
     * 发送文件内容（响应体）：
     * - send_data 内部会先写响应行和响应头，再写文件内容
     */
    send_data(clnt_write, ct, file_name);
}

void send_data(FILE *fp, char *ct, char *file_name)
{
    /*
     * 发送一个最简化的 HTTP 响应：
     * - 响应行：HTTP/1.0 200 OK
     * - 响应头：Server、Content-length、Content-type
     * - 空行（\r\n）后是响应体：文件内容
     *
     * 教学重点：
     * - HTTP 头行以 \r\n 结尾
     * - 头结束后要再加一个 \r\n（即空行）表示头结束
     */
    char protocol[] = "HTTP/1.0 200 OK\r\n";
    char server[] = "Server:Linux Web Server \r\n";

    /*
     * 注意：这里 Content-length 被写死为 2048，这是“示例化/不严谨”的地方
     * - 实际应根据文件真实字节数计算
     * - 否则浏览器可能等待更多数据或认为数据不足
     */
    char cnt_len[] = "Content-length:2048\r\n";

    char cnt_type[SMALL_BUF];
    char buf[BUF_SIZE];
    FILE *send_file;

    /*
     * Content-type 头：
     * - 形如 "Content-type:text/html\r\n\r\n"
     * - 最后的 \r\n\r\n 表示“头结束 + 空行”
     */
    sprintf(cnt_type, "Content-type:%s\r\n\r\n", ct);

    /*
     * 打开要发送的文件：
     * - 以文本方式 "r" 打开（示例只处理文本）
     * - 若文件不存在，返回错误响应
     */
    send_file = fopen(file_name, "r");
    if (send_file == NULL)
    {
        send_error(fp);
        return;
    }

    /* -------- 发送响应头 -------- */
    fputs(protocol, fp);
    fputs(server, fp);
    fputs(cnt_len, fp);
    fputs(cnt_type, fp);

    /* -------- 发送响应体（文件内容） --------
     *
     * 教学重点：这里用 fgets/fputs 逐行发送
     * - 适合纯文本文件
     * - 对二进制文件（图片、压缩包）不适用，会破坏内容
     * - 更通用做法应使用 fread/fwrite 按字节块发送
     *
     * 由于 stdio 有缓冲，示例里每行发送后 fflush(fp)，保证尽快发出
     * （严格来说并不需要每行都 flush，末尾 flush 一次也可，但这里为了演示“立即发送”）
     */
    while (fgets(buf, BUF_SIZE, send_file) != NULL)
    {
        fputs(buf, fp);
        fflush(fp);
    }

    /*
     * 最后再 flush 一次，确保缓冲区内容都写到 socket
     */
    fflush(fp);

    /*
     * 关闭输出流：
     * - fclose(fp) 会关闭它底层的 FD（也就是 dup 出来的 socket FD）
     * - 对客户端而言，服务器发送完毕后关闭连接（HTTP/1.0 常见行为）
     */
    fclose(fp);
}

char *content_type(char *file)
{
    /*
     * 根据文件扩展名返回 MIME 类型（非常简化版）：
     * - xxx.html / xxx.htm -> "text/html"
     * - 其它 -> "text/plain"
     *
     * 教学提示：
     * - 真正的 Web 服务器会支持更多类型（css/js/png/jpg 等）
     * - 且会考虑大小写、无扩展名、多个 '.' 的情况
     */
    char extension[SMALL_BUF];
    char file_name[SMALL_BUF];

    /*
     * 为了避免直接修改传入参数 file，这里先拷贝到本地数组 file_name
     * 因为 strtok 会破坏字符串内容
     */
    strcpy(file_name, file);

    /*
     * strtok(file_name, ".")：以 '.' 分隔
     * - 第一次得到点号前的部分（文件主名），但这里不关心
     * strtok(NULL, ".")：得到扩展名部分（假设只有一个 '.'）
     */
    strtok(file_name, ".");
    strcpy(extension, strtok(NULL, "."));

    if (!strcmp(extension, "html") || !strcmp(extension, "htm"))
        return "text/html";
    else
        return "text/plain";
}

void send_error(FILE *fp)
{
    /*
     * 发送一个非常简化的错误响应（400 Bad Request）：
     * - 响应行：HTTP/1.0 400 Bad Request
     * - 响应头：Server、Content-length、Content-type
     * - 响应体：一段简单的 HTML 错误页面
     *
     * 同样注意：
     * - Content-length 写死为 2048，不严谨
     */
    char protocol[] = "HTTP/1.0 400 Bad Request\r\n";
    char server[] = "Server:Linux Web Server \r\n";
    char cnt_len[] = "Content-length:2048\r\n";
    char cnt_type[] = "Content-type:text/html\r\n\r\n";
    char content[] = "<html><head><title>NETWORK</title></head>"
                     "<body><font size=+5><br>发生错误! 查看请求文件名和请求方式!"
                     "</font></body></html>";

    /*
     * 发送响应行与响应头
     */
    fputs(protocol, fp);
    fputs(server, fp);
    fputs(cnt_len, fp);
    fputs(cnt_type, fp);

    /*
     * 教学提示（不改代码，仅说明）：
     * - 这里并没有把 content（错误页面正文）写出去，浏览器可能收到空正文
     * - 更完整应 fputs(content, fp)
     * - 但题目要求不改代码，所以仅在注释指出
     */
    fflush(fp);
}

void error_handling(char *message)
{
    /*
     * 通用错误处理：
     * - 输出错误信息到 stderr
     * - 换行
     * - 退出进程（exit(1) 表示异常结束）
     */
    fputs(message, stderr);
    fputc('\n', stderr);
    exit(1);
}
