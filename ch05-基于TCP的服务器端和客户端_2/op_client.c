#include <stdio.h>      // printf, fputs, scanf, freopen
#include <stdlib.h>     // exit, atoi
#include <string.h>     // memset
#include <unistd.h>     // read, write, close
#include <arpa/inet.h>  // inet_addr, htons, sockaddr_in
#include <sys/socket.h> // socket, connect

#define BUF_SIZE 1024 // 发送缓冲区大小
#define RLT_SIZE 4    // 结果大小（int 4 字节）
#define OPSZ 4        // 每个操作数占用字节数
void error_handling(char *message);

int main(int argc, char *argv[])
{
    // 将输入输出重定向到文件，方便批量测试
    freopen("1.txt","r", stdin);
    freopen("2.txt", "w", stdout);
    int sock;                 // 客户端套接字描述符
    char opmsg[BUF_SIZE];     // 操作消息缓冲区
    int result, opnd_cnt, i;  // 运算结果、操作数个数、循环计数
    struct sockaddr_in serv_adr; // 服务器地址
    if (argc != 3)
    {
        printf("Usage : %s <IP> <port>\n", argv[0]);
        exit(1);
    }

    // 创建 TCP 套接字
    sock = socket(PF_INET, SOCK_STREAM, 0);
    if (sock == -1)
        error_handling("socket() error");

    // 初始化服务器地址
    memset(&serv_adr, 0, sizeof(serv_adr));
    serv_adr.sin_family = AF_INET;
    serv_adr.sin_addr.s_addr = inet_addr(argv[1]);
    serv_adr.sin_port = htons(atoi(argv[2]));

    // 连接服务器
    if (connect(sock, (struct sockaddr *)&serv_adr, sizeof(serv_adr)) == -1)
        error_handling("connect() error!");
    else
        puts("Connected...........\n");

    // 输入操作数数量
    fputs("Operand count: \n", stdout);
    scanf("%d", &opnd_cnt);
    // 按协议约定：第 1 个字节存放操作数个数
    opmsg[0] = (char)opnd_cnt;

    for (i = 0; i < opnd_cnt; i++)
    {
        printf("Operand %d: \n", i + 1);
        // 操作数以 4 字节 int 形式依次写入缓冲区
        // 注意：此处直接写入主机字节序，要求客户端/服务器字节序一致
        scanf("%d", (int *)&opmsg[i * OPSZ + 1]);
    }
    fgetc(stdin);
    // 输入运算符（+ - *）
    fputs("Operator: ", stdout);
    scanf("%c", &opmsg[opnd_cnt * OPSZ + 1]);
    // 消息格式：| count(1B) | opnd1(4B) | ... | opndN(4B) | op(1B) |
    write(sock, opmsg, opnd_cnt * OPSZ + 2);
    // 读取 4 字节结果
    read(sock, &result, RLT_SIZE);

    printf("Operation result: %d \n", result);
    // 关闭套接字
    close(sock);
    return 0;
}

void error_handling(char *message)
{
    // 将错误信息输出到标准错误并终止程序
    fputs(message, stderr);
    fputc('\n', stderr);
    exit(1);
}
