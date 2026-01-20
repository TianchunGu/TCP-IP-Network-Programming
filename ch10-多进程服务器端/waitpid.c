#include <stdio.h>      // 标准输入输出：printf / puts
#include <unistd.h>     // POSIX：fork / sleep
#include <sys/wait.h>   // 进程等待：waitpid / WNOHANG 以及状态宏 WIFEXITED / WEXITSTATUS

int main(int argc, char* argv[])
{
    int status;              // 用于保存子进程退出状态（由 waitpid 写入）
    pid_t pid = fork();      // 创建子进程
    // 返回值语义：
    // - 子进程中 pid == 0
    // - 父进程中 pid > 0（为子进程 PID）
    // - 失败 pid < 0（此示例未处理失败情况）

    if(pid == 0)
    {
        // -------------------- 子进程分支 --------------------
        // 子进程先睡 15 秒，模拟执行耗时任务
        sleep(15);

        // 在 main 中 return 24 等价于 exit(24)
        // 子进程退出码为 24，父进程可通过 waitpid 得到
        return 24;
    }
    else
    {
        // -------------------- 父进程分支 --------------------
        // waitpid(-1, &status, WNOHANG)：
        // -1      ：等待任意一个子进程（本程序只有一个子进程）
        // &status ：输出参数，用于获取子进程退出状态
        // WNOHANG ：非阻塞选项——如果没有子进程退出，立即返回 0
        //
        // 该循环含义：
        // - 如果子进程还没退出，waitpid 返回 0，
        //   取反后为 1，循环继续；父进程每 3 秒轮询一次。
        // - 一旦子进程退出，waitpid 返回子进程 pid（>0），
        //   !pid 为 0，循环结束。
        //
        // 注意：
        // - 若 waitpid 返回 -1 表示错误（例如没有子进程），!(-1) 为 0，
        //   循环也会退出；此示例未对 -1 做区分处理。
        while (!waitpid(-1, &status, WNOHANG))
        {
            // 子进程未退出时，父进程每次睡 3 秒，避免忙等占用 CPU
            sleep(3);
            puts("sleep 3sec."); // 提示父进程正在轮询等待
        }

        // 循环退出后，说明 waitpid 已经成功回收了一个已退出的子进程（或发生错误返回 -1）
        // WIFEXITED(status)：判断是否为“正常退出”（return 或 exit）
        // 若正常退出，则 WEXITSTATUS(status) 取出退出码（0~255 的低 8 位）
        if(WIFEXITED(status))
            printf("Child send %d\n", WEXITSTATUS(status));
    }

    return 0;
}
