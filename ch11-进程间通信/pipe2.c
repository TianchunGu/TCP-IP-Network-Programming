#include <stdio.h>      // 标准输入输出：printf
#include <unistd.h>     // POSIX：pipe / fork / read / write / sleep 等

#define BUF_SIZE 30     // 管道读缓冲区大小：每次最多读取 30 字节

int main(int argc, char* argv[])
{
    int fds[2];                     // 管道的两个端点：
                                    // fds[0]：读端（read end）
                                    // fds[1]：写端（write end）
    char str1[] = "Who are you?";   // 将由子进程写入管道的第一条消息（字符数组，包含 '\0'）
    char str2[] = "Thank you for your message"; // 将由父进程写入管道的第二条消息（字符数组，包含 '\0'）
    char buf[BUF_SIZE];             // 读缓冲区：用于从管道中读出数据
    pid_t pid;                      // fork 返回值：用于区分父进程与子进程

    // 创建匿名管道（单向字节流 IPC 通道）：
    // - fds[0] 用于读
    // - fds[1] 用于写
    // fork 后父子进程都会继承这两个文件描述符，从而可以通过管道通信
    // 注意：示例未检查 pipe 返回值，真实工程应判断是否创建成功
    pipe(fds);

    // fork：创建子进程
    // 返回值 pid：
    // - 子进程中 pid == 0
    // - 父进程中 pid > 0（为子进程 PID）
    // - 失败 pid < 0（本示例未处理失败情况）
    pid = fork();

    if(pid == 0)
    {
        // -------------------- 子进程分支 --------------------
        // 第一步：向管道写入 str1
        // write(fds[1], str1, sizeof(str1))：
        // - 写端 fds[1]
        // - 写入长度 sizeof(str1) 包含字符串结尾 '\0'
        // 这样对端读出来后可直接当作 C 字符串使用
        write(fds[1], str1, sizeof(str1));

        // sleep(2)：让子进程暂停 2 秒
        // 这里的意图是“给父进程机会先读取 str1，并在稍后写入 str2”
        // 通过时间错开来演示父子进程通过同一管道交换信息
        sleep(2);

        // 第二步：从管道读端读取数据到 buf（期望读到父进程写入的 str2）
        // read 返回实际读取字节数（本示例未保存），最多读 BUF_SIZE 字节
        // 注意：BUF_SIZE=30，而 str2 的长度（含 '\0'）可能大于 30，
        // 因此子进程可能只读到 str2 的一部分，导致输出被截断或不包含 '\0'。
        // 这里依赖“读到的片段碰巧含 '\0'”或终端输出未暴露问题，是教学示例的简化写法。
        read(fds[0], buf, BUF_SIZE);

        // 打印子进程读到的内容
        // 注意：printf("%s") 依赖 buf 内部存在 '\0' 结束符；
        // 如果 read 未读入 '\0'，则可能发生越界读取并输出垃圾数据（示例代码未防护）。
        printf("Child proc output: %s\n", buf);
    }
    else
    {
        // -------------------- 父进程分支 --------------------
        // 第一步：从管道读端读取数据（期望读到子进程先写入的 str1）
        // 由于子进程在 fork 后马上 write(str1)，父进程通常能读到 str1。
        // 同样地，本示例未检查 read 返回值。
        read(fds[0], buf, BUF_SIZE);

        // 打印父进程读到的内容（通常是 "Who are you?"）
        printf("Parent proc output: %s\n", buf);

        // 第二步：父进程向管道写入 str2，供子进程随后读取
        // 写入长度 sizeof(str2) 包含 '\0'
        write(fds[1], str2, sizeof(str2));

        // sleep(3)：父进程暂停 3 秒
        // 主要用于让子进程有时间去 read 并打印 str2，
        // 同时也能让程序输出更稳定、便于观察
        sleep(3);
    }

    // 说明（更严谨的做法，示例未做）：
    // 1) 父子进程应分别 close 掉自己不使用的管道端（例如只读则关闭写端）。
    // 2) 需要保存 read 返回值并手动补 '\0'，以保证 printf/puts 安全。
    // 3) 同一个管道是单向的：两端都可读写只是因为每个进程都继承了读/写描述符；
    //    但这并不等价于“全双工”。要实现可靠的双向通信，通常使用两根管道或 socketpair。
    // 4) str2 可能超过 BUF_SIZE，读写应考虑分段或增大缓冲区。
    return 0;
}
