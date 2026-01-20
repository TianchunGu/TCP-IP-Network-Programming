#include <stdio.h>      // 标准输入输出：printf / puts
#include <unistd.h>     // POSIX：fork / sleep
#include <sys/types.h>  // 类型定义：pid_t

int main(int argc, char* argv[])
{
    // fork：创建一个新的子进程
    // 返回值 pid 的语义：
    // - 在子进程中：pid == 0
    // - 在父进程中：pid > 0（返回新创建子进程的 PID）
    // - 失败时：pid < 0（本示例未处理失败情况）
    pid_t pid = fork();

    // 根据 fork 的返回值区分父/子进程执行不同分支
    if(pid == 0)
    {
        // -------------------- 子进程分支 --------------------
        // puts 会自动在字符串末尾再输出一个 '\n'
        // 这里字符串本身也包含 '\n'，因此可能会输出两个换行（演示用）
        puts("Hi! I'm a child process\n");
    }
    else
    {
        // -------------------- 父进程分支 --------------------
        // pid 是子进程的 PID（正数）
        printf("Child Process ID: %d\n", pid);

        // 父进程睡眠 30 秒：
        // - 常用于演示：让父进程暂时不退出，便于观察进程状态
        // - 在这 30 秒内，子进程可能已经结束；
        //   若父进程未 wait 回收子进程，那么子进程可能在此期间处于“僵尸进程”状态（取决于子进程是否已退出）
        sleep(30);
    }

    // fork 之后，父子进程都会继续执行到这里（除非某个分支提前退出）
    // 因此下面会分别输出“End child process”或“End parent process”
    if(pid == 0)
        puts("End child process");
    else
        puts("End parent process");

    return 0;
}
