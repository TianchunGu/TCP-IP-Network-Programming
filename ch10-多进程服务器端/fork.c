#include <stdio.h>      // 标准输入输出：printf
#include <unistd.h>     // POSIX：fork 等系统调用
#include <sys/types.h>  // 数据类型定义：pid_t 等

int gval = 10;          // 全局变量：位于数据段（data segment），fork 后父子进程各自拥有一份副本（写时复制）

int main(int argc, char* argv[])
{
    pid_t pid;          // 用于保存 fork() 的返回值：区分父进程和子进程
    int lval = 20;      // 局部变量：位于栈上，fork 后父子进程也各自拥有一份副本（写时复制）

    // 在 fork 之前先修改全局变量与局部变量
    // 这一步在父进程中执行一次，fork 之后子进程会继承“此刻”的内存状态
    gval++;             // gval: 10 -> 11
    lval += 5;          // lval: 20 -> 25

    // fork：创建子进程
    // 返回值 pid 的语义：
    // - 在子进程中返回 0
    // - 在父进程中返回子进程的 PID（>0）
    // - 失败返回 -1（本示例未显式处理失败情况）
    pid = fork();

    // 根据当前进程身份（父/子）分别修改变量
    // 重点：父子进程的地址空间相互独立，下面的修改互不影响
    if(pid == 0)
        // 子进程分支：在子进程自己的副本中修改 gval/lval
            // 逗号运算符：先执行 gval += 2，再执行 lval += 2
                gval += 2, lval += 2;
    else
        // 父进程分支：在父进程自己的副本中修改 gval/lval
            // 同样使用逗号运算符把两条表达式写在一行
                gval -= 2, lval -=2;

    // 分别在父/子进程中打印各自的 gval 与 lval
    // 由于 fork 之后父子进程独立运行，输出的先后顺序不确定（由调度决定）
    if(pid == 0)
        printf("Child Proc: gval: %d, lval: %d\n", gval, lval);
    else
        printf("Parent Proc: gval: %d, lval: %d\n", gval, lval);

    return 0;
}
