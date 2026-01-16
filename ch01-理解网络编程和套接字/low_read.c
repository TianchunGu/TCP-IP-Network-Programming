#include <stdio.h>  // printf, fputs, stderr
#include <stdlib.h> // exit
#include <fcntl.h>  // open, O_RDONLY
#include <unistd.h> // read, close

#define BUF_SIZE 100 // 本例中一次性读取的最大字节数
void error_handling(char *message);

int main(int argc, char* argv[])
{
    int fd;             // open() 返回的文件描述符
    char buf[BUF_SIZE]; // 用于承载文件内容的缓冲区

    // 以只读方式打开文件 data.txt
    fd = open("data.txt", O_RDONLY);
    if(fd == -1)
        error_handling("open() error!");
    printf("File Descriptor: %d\n", fd);

    // 从文件中读取数据到缓冲区，最多读 BUF_SIZE 字节
    if(read(fd, buf, sizeof(buf)) == -1)
        error_handling("read() error!");
    
    // 本例假定 data.txt 是文本数据，可直接作为字符串输出
    printf("file data: %s\n", buf);

    // 关闭文件描述符，释放系统资源
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
