#include <stdio.h>      // printf, fputs
#include <stdlib.h>     // exit, atoi
#include <string.h>     // memset
#include <unistd.h>     // read, write, close
#include <arpa/inet.h>  // htonl, htons, sockaddr_in
#include <sys/socket.h> // socket, bind, listen, accept

#define BUF_SIZE 1024 // 接收缓冲区大小
#define OPSZ 4        // 每个操作数占用字节数
void error_handling(char *message);
int calculate(int opnum, int opnds[], char operator);

int main(int argc, char* argv[])
{
    int serv_sock, clnt_sock;      // 监听套接字与客户端连接套接字
    char opinfo[BUF_SIZE];         // 操作数与运算符缓冲区
    int result, opnd_cnt, i;       // 运算结果、操作数个数、循环计数
    int recv_cnt, recv_len;        // 单次接收长度与累计接收长度
    struct sockaddr_in serv_addr, clnt_addr; // 服务器与客户端地址
    socklen_t clnt_addr_sz;        // 客户端地址结构体长度
    if(argc != 2)
    {
        printf("Usage: %s <port>\n", argv[0]);
        exit(1);
    }

    // 创建 TCP 监听套接字
    serv_sock = socket(PF_INET, SOCK_STREAM, 0);
    if(serv_sock == -1)
        error_handling("socket() error");
    
    // 初始化并绑定服务器地址
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(atoi(argv[1]));

    // 绑定与监听
    if(bind(serv_sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == -1)
        error_handling("bind() error");
    if(listen(serv_sock, 5) == -1)
        error_handling("listen() error");
    clnt_addr_sz = sizeof(clnt_addr);

    // 依次处理最多 5 个客户端
    for(i = 0; i < 5; i++)
    {
        opnd_cnt = 0;
        // 阻塞等待客户端连接
        clnt_sock = accept(serv_sock, (struct sockaddr*)&serv_addr, &clnt_addr_sz);
        if(clnt_sock == -1)
            error_handling("accept() error");
        
        // 先读取 1 字节的操作数个数
        read(clnt_sock, &opnd_cnt, 1);

        recv_len = 0;
        // 再读取 操作数( opnd_cnt * 4 ) + 运算符(1) 的剩余数据
        while(recv_len < (opnd_cnt * OPSZ + 1))
        {
            recv_cnt = read(clnt_sock, &opinfo[recv_len], BUF_SIZE - 1);
            recv_len += recv_cnt;
        }
        // opinfo 前面是操作数数组，最后 1 字节是运算符
        result = calculate(opnd_cnt, (int*)opinfo, opinfo[recv_len - 1]);
        // 返回 4 字节计算结果
        write(clnt_sock, (char*)&result, sizeof(result));
        // 关闭客户端连接
        close(clnt_sock);
    }
    // 关闭监听套接字
    close(serv_sock);
    
    return 0;
}

int calculate(int opnum, int opnds[], char op)
{
    // 以第一个操作数为初始值
    int result = opnds[0];

    switch (op)
    {
    case '+':
        // 依次相加
        for(int i = 1; i < opnum; i++) result += opnds[i];
        break;
    case '-':
        // 依次相减
        for(int i = 1; i < opnum; i++) result -= opnds[i];
        break;
    case '*':
        // 依次相乘
        for(int i = 1; i < opnum; i++) result *= opnds[i];
        break;
    default:
        break;
    }

    return result;
}

void error_handling(char *message)
{
	// 将错误信息输出到标准错误并终止程序
	fputs(message, stderr);
	fputc('\n', stderr);
	exit(1);
}
