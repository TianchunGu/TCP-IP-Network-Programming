#include <stdio.h>      // 标准输入输出：printf
#include <unistd.h>     // POSIX：pipe / fork / read / write / sleep 等

#define BUF_SIZE 30     // 每次从管道读取的最大字节数

int main(int argc, char* argv[])
{
    int fds1[2], fds2[2];            // 两根管道：
                                     // fds1：用于 子进程 -> 父进程 的数据传输
                                     //   fds1[0]：读端（父进程读）
                                     //   fds1[1]：写端（子进程写）
                                     // fds2：用于 父进程 -> 子进程 的数据传输
                                     //   fds2[0]：读端（子进程读）
                                     //   fds2[1]：写端（父进程写）

    char str1[] = "Who are you?";    // 子进程发送给父进程的消息（字符数组，包含 '\0'）
    char str2[] = "Thank you for your message"; // 父进程发送给子进程的消息（字符数组，包含 '\0'）
    char buf[BUF_SIZE];              // 接收缓冲区：用于从管道读取数据
    pid_t pid;                       // fork 返回值：区分父/子进程

    // -------------------- 创建两根匿名管道 --------------------
    // pipe(fds) 创建单向字节流通信通道：
    // - fds[0]：读端
    // - fds[1]：写端
    //
    // 由于“管道本质上是单向的”，若要实现父子进程双向通信，
    // 通常需要两根管道：一根 A->B，另一根 B->A（本程序正是如此）
    pipe(fds1);
    pipe(fds2);

    // fork：创建子进程
    // 返回值 pid：
    // - 子进程中 pid == 0
    // - 父进程中 pid > 0（为子进程 PID）
    // - 失败 pid < 0（本示例未处理失败情况）
    pid = fork();

    if(pid == 0)
    {
        // ==================== 子进程逻辑 ====================

        // 第一步：子进程通过 fds1 的写端 fds1[1] 向父进程发送 str1
        // sizeof(str1) 包含结尾 '\0'，便于父进程读出后直接按字符串输出
        write(fds1[1], str1, sizeof(str1));

        // 第二步：子进程从 fds2 的读端 fds2[0] 读取父进程回送的消息到 buf
        // 最多读 BUF_SIZE 字节
        //
        // 注意：
        // - str2 可能比 BUF_SIZE 更长（含 '\0' 后长度也可能 >30），
        //   因此这里可能只读到 str2 的前一部分，导致输出被截断或缺少 '\0'。
        // - 本示例未保存 read 的返回值，也未手动补 '\0'，属于教学简化写法。
        read(fds2[0], buf, BUF_SIZE);

        // 打印子进程读到的内容
        // printf("%s") 依赖 buf 中存在 '\0' 结束符；若不存在可能越界读取（示例未防护）
        printf("Child proc output: %s\n", buf);
    }
    else
    {
        // ==================== 父进程逻辑 ====================

        // 第一步：父进程从 fds1 的读端 fds1[0] 读取子进程发来的 str1
        // 通常能读到 "Who are you?"（含 '\0'），因此可以直接以字符串打印
        read(fds1[0], buf, BUF_SIZE);

        // 打印父进程读到的内容
        printf("Parent proc output: %s\n", buf);

        // 第二步：父进程通过 fds2 的写端 fds2[1] 向子进程发送 str2
        // sizeof(str2) 包含 '\0'
        write(fds2[1], str2, sizeof(str2));

        // sleep(1)：让父进程稍微停一下，给子进程时间读取并打印
        // （否则父进程可能很快退出，虽然子进程通常仍会继续运行，但示例中用 sleep 便于观察）
        sleep(1);
    }

    // 说明（更严谨的做法，本示例未做）：
    // 1) 每个进程应关闭自己不使用的管道端，避免资源泄漏、避免读端无法收到 EOF：
    //    - 子进程通常关闭 fds1[0]、fds2[1]
    //    - 父进程通常关闭 fds1[1]、fds2[0]
    // 2) 应保存 read 的返回值 n，然后做 buf[n] = '\0' 以保证字符串输出安全。
    // 3) 若消息可能超过 BUF_SIZE，应循环 read 或增大缓冲区，或使用更可靠的协议/长度字段。
    return 0;
}
