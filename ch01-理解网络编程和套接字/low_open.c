#include <stdio.h>  // printf, fputs, stderr
#include <stdlib.h> // exit
#include <fcntl.h>  // open, O_CREAT, O_WRONLY, O_TRUNC
#include <unistd.h> // write, close

void error_handling(char *message);

int main(void)
{
    int fd;                   // open() 返回的文件描述符
    char buf[] = "Let's go!\n"; // 即将写入文件的数据

    // 若文件不存在则创建；以只写方式打开；存在则清空原内容
    fd = open("data.txt", O_CREAT | O_WRONLY | O_TRUNC);
    if(fd == -1)
        error_handling("open() error!");
    printf("file descriptor: %d \n", fd);

    // 将缓冲区内容写入文件
    if(write(fd, buf, sizeof(buf)) == -1)
        error_handling("write() error!");

    // 关闭文件描述符，确保数据落盘并释放资源
    close(fd);
    return 0;
}

void error_handling(char *message)
{
    // 将错误信息输出到标准错误并终止程序
    fputs(message, stderr);
    fputc('\n', stderr);
    exit(1);
}
