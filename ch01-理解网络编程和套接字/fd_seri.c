#include <stdio.h>      // printf
#include <fcntl.h>      // open, O_CREAT, O_WRONLY, O_TRUNC
#include <unistd.h>     // close
#include <sys/socket.h> // socket

int main(void)
{
    int fd1, fd2, fd3; // 操作系统分配的文件/套接字描述符
    // 创建 TCP 套接字，用于观察描述符分配顺序
    fd1 = socket(PF_INET, SOCK_STREAM, 0);
    // 创建或截断文件并以只写方式打开
    fd2 = open("test.dat", O_CREAT | O_WRONLY | O_TRUNC);
    // 创建 UDP 套接字，继续观察描述符分配
    fd3 = socket(PF_INET, SOCK_DGRAM, 0);

    // 打印描述符编号，通常会递增分配（具体由系统决定）
    printf("file descriptor 1: %d\n", fd1);
    printf("file descriptor 2: %d\n", fd2);
    printf("file descriptor 3: %d\n", fd3);

    // 关闭所有描述符，释放系统资源
    close(fd1);
    close(fd2);
    close(fd3);

    return 0;
}
