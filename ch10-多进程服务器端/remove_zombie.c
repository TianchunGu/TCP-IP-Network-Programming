#include <stdio.h>      // 标准输入输出：printf / puts 等
#include <stdlib.h>     // 标准库：exit 等
#include <unistd.h>     // POSIX：fork / sleep 等
#include <signal.h>     // 信号处理：sigaction / sigemptyset / SIGCHLD 等
#include <sys/wait.h>   // 子进程回收：waitpid / WNOHANG / WIFEXITED / WEXITSTATUS 等宏

// -------------------- SIGCHLD 信号处理函数 --------------------
// 当子进程退出时，内核会向父进程发送 SIGCHLD 信号。
// 父进程如果不调用 wait/waitpid 回收子进程，子进程就会成为“僵尸进程”（占用进程表项）。
void read_childproc(int sig)
{
    int status;  // 用于接收子进程的退出状态信息（由 waitpid 写入）

    // waitpid(-1, &status, WNOHANG)：
    // -1      ：等待任意一个子进程
    // &status ：输出参数，保存子进程退出状态
    // WNOHANG ：非阻塞；如果没有已退出的子进程，立即返回 0
    //
    // 返回值 id：
    // >0：成功回收一个子进程，返回该子进程 pid
    //  0：当前没有可回收的子进程（非阻塞立即返回）
    // -1：出错（例如没有子进程等）
    pid_t id = waitpid(-1, &status, WNOHANG);

    // WIFEXITED(status)：判断子进程是否“正常结束”（通过 return 或 exit 退出）
    // 若是正常结束，则可用 WEXITSTATUS(status) 取出退出码（0~255 的低 8 位）
    if(WIFEXITED(status))
    {
        printf("Removed proc id: %d\n", id);              // 打印被回收的子进程 pid
        printf("Child send: %d\n", WEXITSTATUS(status)); // 打印子进程的退出码（例如 return 12 或 exit(24)）
    }

    // 说明：
    // - 若同时有多个子进程退出，SIGCHLD 可能合并触发；
    //   严谨做法常用 while(waitpid(-1, &status, WNOHANG) > 0) 循环回收所有已退出子进程。
    // - 在信号处理函数里调用 printf 并非严格意义的“异步信号安全”（示例代码为了演示方便）。
}

int main(int argc, char* argv[])
{
    pid_t pid;               // 保存 fork() 返回值，用于区分父/子进程
    struct sigaction act;    // 用于配置信号处理动作（handler、mask、flags 等）

    // -------------------- 注册 SIGCHLD 处理函数 --------------------
    act.sa_handler = read_childproc; // SIGCHLD 到来时调用 read_childproc
    sigemptyset(&act.sa_mask);       // 处理 SIGCHLD 时不额外屏蔽其他信号
    act.sa_flags = 0;                // 标志位：此处不设置 SA_RESTART 等
    sigaction(SIGCHLD, &act, 0);     // 安装 SIGCHLD 信号处理器
    // 注意：此处未检查 sigaction 返回值；真实工程应判断是否安装成功

    // -------------------- 创建第一个子进程 --------------------
    pid = fork();

    // fork 返回值语义：
    // - 子进程中 pid == 0
    // - 父进程中 pid > 0（为子进程 pid）
    // - 失败 pid < 0（本示例未处理失败情况）
    if(pid == 0)
    {
        // 子进程 1：打印提示，休眠 10 秒，然后用 return 12 结束
        // return 12 等价于调用 exit(12)（在 main 中）
        puts("Hi! I'm child process");
        sleep(10);   // 模拟子进程运行一段时间
        return 12;   // 子进程退出码 12（会被父进程的 SIGCHLD 处理器读取并打印）
    }
    else
    {
        // 父进程：打印第一个子进程 pid
        printf("Child proc id: %d\n", pid);

        // -------------------- 创建第二个子进程 --------------------
        pid = fork();

        if(pid == 0)
        {
            // 子进程 2：打印提示，休眠 10 秒，然后用 exit(24) 结束
            puts("Hi! I'm child process");
            sleep(10);  // 模拟子进程运行
            exit(24);   // 子进程退出码 24
        }
        else
        {
            // 父进程：打印第二个子进程 pid
            printf("Child proc id: %d\n", pid);

            // 父进程做一些“等待中的工作”，每 5 秒打印一次，共 5 次
            // 在此期间，两个子进程会先后退出并触发 SIGCHLD，
            // read_childproc 会被调用来回收子进程并输出其退出码
            for(int i = 0; i < 5; i++)
            {
                puts("Wait...");
                sleep(5);
            }
        }
    }

    // 父进程结束时，如果子进程尚未退出，仍会继续运行；
    // 但在本程序中父进程至少会运行约 25 秒，足够两个子进程（各睡 10 秒）先退出并被回收。
    return 0;
}
