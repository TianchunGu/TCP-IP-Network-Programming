#include <stdio.h>      // 标准输入输出：puts
#include <unistd.h>     // POSIX：alarm / sleep
#include <signal.h>     // 信号处理：sigaction / sigemptyset / SIGALRM 等

// -------------------- SIGALRM 的信号处理函数 --------------------
// 当闹钟到期（alarm 设定的秒数到了）时，内核会向进程发送 SIGALRM 信号，
// 然后会调用这里注册的处理函数 timeout。
void timeout(int sig)
{
    // sig 参数是触发该处理函数的信号编号
    // 这里做一次判断：只有当信号确实是 SIGALRM 时才输出提示
    if(sig == SIGALRM)
        puts("Time out!");

    // 重新设置闹钟为 2 秒：
    // 这相当于“周期性定时器”的效果——每次 SIGALRM 来了，再安排下一次 SIGALRM
    // 注意：alarm 会覆盖之前尚未到期的 alarm 设置
    alarm(2);
}

int main(int argc, char *argv[])
{
    struct sigaction act;  // 用于配置信号处理方式的结构体（handler、mask、flags 等）

    // -------------------- 安装 SIGALRM 的信号处理器 --------------------
    act.sa_handler = timeout;     // 指定当 SIGALRM 到来时调用 timeout
    sigemptyset(&act.sa_mask);    // 清空信号屏蔽集：处理 SIGALRM 时不额外阻塞其他信号
    act.sa_flags = 0;             // 标志位：此处不设置 SA_RESTART 等
    sigaction(SIGALRM, &act, 0);  // 注册 SIGALRM 的处理动作
    // 注意：示例代码未检查 sigaction 返回值；真实工程中应判断是否安装成功

    // -------------------- 启动第一次闹钟 --------------------
    // 2 秒后进程会收到 SIGALRM，timeout() 被调用
    alarm(2);

    // -------------------- 主循环：演示 sleep 被信号中断的效果 --------------------
    for(int i = 0; i < 3; i++)
    {
        puts("Wait...");

        // sleep(100)：期望睡眠 100 秒
        // 但如果在睡眠期间收到了信号（如 SIGALRM），sleep 可能会被中断并提前返回。
        // 由于我们每 2 秒就会收到一次 SIGALRM，因此 sleep(100) 实际上会不断被打断。
        //
        // 同时，由于 timeout() 里再次 alarm(2)，会持续产生 SIGALRM，
        // 所以这里的循环会在不断的 “Wait...” 和 “Time out!” 之间交替（实际输出顺序取决于调度）
        sleep(100);
    }

    return 0;
}
