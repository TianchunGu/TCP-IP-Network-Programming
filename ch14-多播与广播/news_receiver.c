#include <stdio.h>      // 标准I/O：printf, fputs, stdout, stderr
#include <stdlib.h>     // exit, atoi
#include <string.h>     // memset
#include <unistd.h>     // close
#include <arpa/inet.h>  // inet_addr, htonl, htons：IP/端口转换、字节序转换
#include <sys/socket.h> // socket, bind, recvfrom, setsockopt 等套接字接口

#define BUF_SIZE 30     // 接收缓冲区大小（示例较小，便于演示）

void error_handling(char *message);

int main(int argc, char *argv[])
{
    int recv_sock;                  // 接收端 UDP socket（文件描述符）
    int str_len;                    // 实际接收到的字节数
    char buf[BUF_SIZE];             // 接收缓冲区
    struct sockaddr_in adr;         // 本地地址（用于 bind）
    struct ip_mreq join_adr;        // 多播组加入信息结构体（IPv4 Multicast Request）

    /*
     * 命令行参数说明（按 Usage 的语义）：
     * - argv[1]：GroupIP（要加入的多播组 IP，一般为 D 类地址 224.0.0.0 ~ 239.255.255.255）
     * - argv[2]：PORT（多播发送端使用的 UDP 端口；接收端必须绑定同一个端口才能收到）
     *
     * 注意：你这段代码里 argv[1]/argv[2] 的使用位置有“对调”的现象（见下面注释）
     */
    if(argc!=3) {
        /*
         * 教学提示：
         * - argv 是 char* argv[]，这里打印 argv 会是地址（不符合本意）
         * - 正确通常打印 argv[0]（程序名）
         * 但题目要求不改代码，这里只做解释。
         */
        printf("Usage : %s <GroupIP> <PORT>\n", argv);
        exit(1);
    }

    /* ======================= 1. 创建 UDP 套接字 =======================
     *
     * socket(PF_INET, SOCK_DGRAM, 0)
     * - PF_INET：IPv4
     * - SOCK_DGRAM：UDP（无连接、数据报）
     * - 0：自动选择协议（对于 IPv4 + DGRAM 通常就是 UDP）
     *
     * 教学重点：多播接收通常使用 UDP
     * - IP 多播在应用层最常见的模式就是 UDP 多播
     * - 接收端不需要 connect，只要 bind 端口并加入组即可收包
     */
    recv_sock=socket(PF_INET, SOCK_DGRAM, 0);

    /* ======================= 2. 绑定本地端口 =======================
     *
     * UDP 接收必须绑定端口：
     * - 不 bind，内核不知道把目标端口的数据交给哪个 socket
     * - 多播接收也是一样：报文目的端口必须匹配 bind 的端口
     */
    memset(&adr, 0, sizeof(adr));
    adr.sin_family=AF_INET;
    adr.sin_addr.s_addr=htonl(INADDR_ANY); // INADDR_ANY：绑定本机所有网卡地址（0.0.0.0）

    /*
     *解析端口号
     */
    adr.sin_port=htons(atoi(argv[2]));     // 注：这里理论上应与 Sender 的端口一致

    /*
     * bind：把 recv_sock 绑定到本地地址 adr（0.0.0.0:PORT）
     * - 成功返回 0
     * - 失败返回 -1（常见原因：端口被占用、权限不足等）
     */
    if(bind(recv_sock, (struct sockaddr*) &adr, sizeof(adr))==-1)
        error_handling("bind() error");

    /* ======================= 3. 配置要加入的多播组（ip_mreq） =======================
     *
     * struct ip_mreq 常用字段：
     * - imr_multiaddr：要加入的多播组地址（D类地址）
     * - imr_interface：本机使用哪个网卡接口加入该多播组
     *
     * 教学重点：什么叫“加入多播组”？
     * - 多播报文目标地址是一个组地址（如 224.1.1.2）
     * - 主机必须显式告诉内核：我想加入这个组（IP_ADD_MEMBERSHIP）
     * - 内核会配置本机网卡对该组地址的接收（通常涉及 IGMP 协议与路由器交互）
     * - 只有加入组的主机才会把该组的报文上交给 socket
     */

    /*
     * !!! 重要：这里 join_adr.imr_multiaddr.s_addr = inet_addr(argv[2]);
     * 按 Usage 约定 argv[2] 是 PORT，不是 GroupIP。
     * 也就是说：此处“把端口号当成多播IP”去 inet_addr，会得到 INADDR_NONE 等错误结果。
     *
     * 正常逻辑应该是：inet_addr(argv[1]) 用于组IP
     * 但同样不改代码，只在注释中指出。
     */
    join_adr.imr_multiaddr.s_addr=inet_addr(argv[1]); // 注：这里理论上应是 argv[1]（GroupIP）

    /*
     * imr_interface 设置为 INADDR_ANY：
     * - 表示由内核自行选择默认的本地接口来加入多播组
     * - 若机器有多块网卡、或你希望指定某块网卡（如 192.168.1.10），
     *   则需要把该网卡IP填入 imr_interface.s_addr。
     */
    join_adr.imr_interface.s_addr=htonl(INADDR_ANY);

    /* ======================= 4. 加入多播组（关键 setsockopt） =======================
     *
     * setsockopt(recv_sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, ...)
     *
     * - level = IPPROTO_IP：表示设置的是 IPv4 层面的选项
     * - optname = IP_ADD_MEMBERSHIP：加入多播组
     * - optval 指向 ip_mreq 结构体
     *
     * 教学重点：
     * - 这是多播接收端必做的一步
     * - 否则即使 bind 了端口，也通常收不到发往组地址的多播包
     *
     * 教学提示（不改代码，仅说明）：
     * - 这里没有检查 setsockopt 的返回值
     * - 严谨实现应判断返回值是否为 -1，并用 perror 输出原因
     */
    setsockopt(recv_sock, IPPROTO_IP, IP_ADD_MEMBERSHIP,
               (void*)&join_adr, sizeof(join_adr));

    /* ======================= 5. 循环接收多播 UDP 数据 =======================
     *
     * 多播基于 UDP：
     * - 使用 recvfrom 接收数据报
     * - recvfrom 每次返回一整个 UDP 数据报的内容（如果缓冲区足够大）
     * - 若缓冲区不够大，超出的部分会被截断（UDP 不会帮你“分段重组到应用层”）
     */
    while(1)
    {
        /*
         * recvfrom(recv_sock, buf, BUF_SIZE-1, 0, NULL, 0)
         * - 第四个参数 flags=0：默认阻塞接收
         * - 最后两个参数传 NULL 和 0：表示不关心发送者地址信息
         *
         * 教学重点：
         * - 如果想知道是哪台机器发来的，可以传入 sockaddr_in 结构体接收源地址
         */
        str_len=recvfrom(recv_sock, buf, BUF_SIZE-1, 0, NULL, 0);
        if(str_len<0)
            break; // 接收出错则退出循环

        /*
         * 把接收到的字节当作字符串输出：
         * - 手动补 '\0' 作为 C 字符串结束符
         * - 这要求发送端发送的内容本质上是文本，否则输出可能乱码
         */
        buf[str_len]=0;
        fputs(buf, stdout);
    }

    /*
     * 关闭 socket，释放系统资源
     */
    close(recv_sock);
    return 0;
}

void error_handling(char *message)
{
    /*
     * 简单错误处理：
     * - 输出错误信息到 stderr
     * - 换行
     * - 退出程序
     *
     * 教学提示：
     * - 这里没有输出 errno 的具体原因
     * - 更完整的写法通常会用 perror(message)
     */
    fputs(message, stderr);
    fputc('\n', stderr);
    exit(1);
}
