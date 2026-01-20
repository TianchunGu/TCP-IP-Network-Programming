#include <stdio.h>      // 标准输入输出：puts
#include <unistd.h>     // POSIX：alarm / sleep
#include <signal.h>     // 信号处理：signal / SIGALRM / SIGINT 等

// -------------------- SIGALRM 的信号处理函数 --------------------
// SIGALRM：由 alarm() 定时到期触发的信号。
// 当进程收到 SIGALRM 时，会调用该函数。
void timeout(int sig)
{
    // 判断触发信号是否为 SIGALRM（这里用于演示，实际情况下注册到 SIGALRM 基本只会收到它）
    if(sig == SIGALRM)
        puts("Time out!");

    // 重新设置闹钟为 2 秒，形成“周期性”触发效果：
    // 每次 SIGALRM 来了，就再安排 2 秒后再次发送 SIGALRM
    alarm(2);
}

// -------------------- SIGINT 的信号处理函数 --------------------
// SIGINT：通常由用户在终端按下 Ctrl+C 触发（中断信号）。
// 默认行为是终止进程；此处注册处理函数后，就会改为执行自定义逻辑。
void keycontrol(int sig)
{
    // 判断是否为 SIGINT（同样是演示性的判断）
    if(sig == SIGINT)
        puts(" Ctrl+C pressed");
}

int main(int argc, char* argv[])
{
    // -------------------- 使用 signal() 注册信号处理函数 --------------------
    // signal 是较早的信号注册接口：
    // - signal(SIGALRM, timeout)：SIGALRM 到来时调用 timeout
    // - signal(SIGINT, keycontrol)：SIGINT 到来时调用 keycontrol
    //
    // 注意：
    // - signal 的语义在不同系统上曾存在差异，现代更推荐使用 sigaction 更可控
    // - 本示例用于展示两个信号的处理：定时信号与键盘中断信号
    signal(SIGALRM, timeout);
    signal(SIGINT, keycontrol);

    // -------------------- 启动第一次闹钟 --------------------
    // 2 秒后产生 SIGALRM，timeout() 会被调用
    alarm(2);

    // -------------------- 主循环：演示 sleep 被信号打断 --------------------
    for(int i = 0; i < 3; i++)
    {
        puts("Wait...");

        // sleep(100)：期望睡眠 100 秒
        // 但在睡眠期间如果收到信号（例如每 2 秒一次的 SIGALRM，或用户按 Ctrl+C 产生 SIGINT），
        // sleep 可能会被中断并提前返回。
        //
        // 因为 timeout() 中每次都会再次 alarm(2)，所以 SIGALRM 会持续周期性到来，
        // 导致 sleep(100) 频繁被中断，程序会不断输出：
        //   Wait...
        //   Time out!
        // 若用户按下 Ctrl+C，还会额外输出：
        //   Ctrl+C pressed
        //
        // 输出的具体交错顺序由进程调度与信号到达时机决定。
        sleep(100);
    }

    return 0;
}
