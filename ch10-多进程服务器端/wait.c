#include <stdio.h>      // 标准输入输出：printf
#include <stdlib.h>     // 标准库：exit
#include <unistd.h>     // POSIX：fork / sleep
#include <sys/wait.h>   // 进程等待与状态宏：wait / WIFEXITED / WEXITSTATUS 等

int main(int argc, char* argv[])
{
    int status;              // 用于保存 wait() 返回的子进程退出状态（由内核写入）
    pid_t pid = fork();      // 第一次 fork：创建第一个子进程
                             // pid 在子进程中为 0，在父进程中为子进程 PID（>0）

    if(pid == 0)
    {
        // -------------------- 子进程 1 --------------------
        // 在 main 中 return 3：等价于 exit(3)
        // 退出码 3 会被父进程通过 wait() 回收时得到（WEXITSTATUS）
        return 3;
    }
    else
    {
        // -------------------- 父进程（第一次 fork 的父进程分支） --------------------
        printf("Child PID: %d\n", pid); // 打印第一个子进程的 PID

        // 第二次 fork：父进程再创建第二个子进程
        pid = fork();

        if(pid == 0)
        {
            // -------------------- 子进程 2 --------------------
            // 显式调用 exit(7) 退出，退出码为 7
            // 父进程后续 wait() 回收时可获得该退出码
            exit(7);
        }
        else
        {
            // -------------------- 父进程（第二次 fork 的父进程分支） --------------------
            printf("Child PID: %d\n", pid); // 打印第二个子进程的 PID

            // wait(&status)：阻塞等待任意一个子进程结束
            // 返回值是被回收子进程的 PID（这里未保存返回值）
            // status 中包含该子进程的终止信息（正常退出/信号终止等）
            wait(&status);

            // WIFEXITED(status)：判断子进程是否“正常结束”（通过 return 或 exit）
            // 若正常结束，则 WEXITSTATUS(status) 可取出退出码（0~255 的低 8 位）
            if(WIFEXITED(status))
                printf("Child send one: %d\n", WEXITSTATUS(status));

            // 再次 wait(&status)：回收另一个尚未回收的子进程
            // 注意：两次 wait 的回收顺序不保证与 fork 顺序一致，
            // 哪个子进程先退出，就可能先被 wait 回收。
            wait(&status);
            if(WIFEXITED(status))
                printf("Child send two: %d\n", WEXITSTATUS(status));

            // 父进程额外睡眠 30 秒：
            // - 常用于演示：父进程不立即结束，便于观察进程状态（例如通过 ps/top）
            // - 此时两个子进程已经被 wait 回收，不会成为僵尸进程
            sleep(30);
        }
    }

    return 0; // 父进程正常退出
}
