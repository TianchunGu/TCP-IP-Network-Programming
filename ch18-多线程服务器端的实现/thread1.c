#include <stdio.h>     // puts：向标准输出打印字符串（自带换行）
#include <unistd.h>    // sleep：让当前线程休眠指定秒数（单位：秒）
#include <pthread.h>   // pthread_create, pthread_t：POSIX 线程接口

/*
 * 线程函数声明：
 * - thread_main：线程入口函数（线程启动后从这里开始执行）
 * 线程入口函数的固定签名通常是：
 *   void* func(void* arg)
 * 其中 arg 用来接收创建线程时传入的参数指针。
 */
void* thread_main(void* arg);

int main(int argc, char* argv[])
{
    pthread_t t_id;
    /*
     * pthread_t：线程标识类型
     * t_id：用于保存新创建线程的 ID，便于后续 join / detach / 取消等操作
     */

    int thread_param = 5;
    /*
     * thread_param：传给线程的参数
     * - 这里值为 5，表示线程循环打印 5 次
     * - 注意：它是 main 函数的局部变量，位于 main 线程栈上
     *
     * 教学重点（生命周期与安全性）：
     * - 把局部变量的地址传给线程，必须保证：
     *   线程在使用该地址期间，该变量仍然有效（main 尚未返回，栈帧尚未销毁）
     * - 本程序中 main 会 sleep(10)，而线程只 sleep(1)*5=5 秒即可结束，
     *   因此线程读取 thread_param 时变量仍然存在 -> 这是安全的
     * - 如果 main 很快结束，线程仍在运行，可能读取到已失效的栈地址，造成未定义行为
     */

    /*
     * pthread_create(&t_id, NULL, thread_main, (void*)&thread_param)
     *
     * 参数说明：
     * 1) &t_id：输出参数，保存新线程 ID
     * 2) NULL：线程属性，NULL 表示使用默认属性
     * 3) thread_main：线程入口函数（新线程将从这里开始执行）
     * 4) (void*)&thread_param：传给线程函数的参数指针（类型统一为 void*）
     *
     * 返回值：
     * - 0：创建成功
     * - 非 0：失败（返回错误码）
     */
    if(pthread_create(&t_id, NULL, thread_main, (void*)&thread_param) != 0)
    {
        puts("pthread_create() error");
        return -1; // 创建失败直接返回
    }

    /*
     * sleep(10)：让 main 线程休眠 10 秒
     *
     * 教学重点：为什么这里要 sleep？
     * - 本程序没有 pthread_join(t_id, ...) 等待线程结束
     * - 如果 main 线程立刻 return，整个进程会结束，子线程也会被系统直接终止
     * - 为了让子线程有机会运行并输出，这里让 main 线程暂停 10 秒
     *
     * 更规范的写法通常是：
     * - pthread_join(t_id, NULL) 等待线程结束
     * 或
     * - pthread_detach(t_id) 把线程设为分离态，让它自己结束并回收资源
     * 但题目要求不改代码，这里只解释现象。
     */
    sleep(10);

    /*
     * main 线程休眠结束后打印一句话
     * 此时子线程理论上已经打印完 5 次 "running thread"
     */
    puts("end of main");

    /*
     * main 返回后进程结束：
     * - 如果子线程还没执行完，会被强制终止
     * - 本程序通过 sleep(10) 避免了这种情况（子线程 5 秒就结束了）
     */
    return 0;
}

void *thread_main(void* arg)
{
    /*
     * 线程入口函数：
     * arg 是 void*，实际传入的是 &thread_param
     * 因此需要强转回 int* 再解引用取值
     */
    int cnt = *((int *)arg);

    /*
     * 循环 cnt 次，每次：
     * - sleep(1)：休眠 1 秒
     * - puts("running thread")：打印一句话
     *
     * 因为 cnt=5，所以总共会打印 5 次，大约耗时 5 秒
     */
    for(int i = 0; i < cnt; i++)
    {
        sleep(1);
        puts("running thread");
    }

    /*
     * 线程函数返回：
     * - 返回值类型是 void*，可用于 pthread_join 获取线程的“退出码/结果”
     * - 本例返回 NULL 表示没有特别返回值
     */
    return NULL;
}
