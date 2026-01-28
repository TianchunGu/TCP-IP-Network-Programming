#include <stdio.h>      // printf, puts：标准输出
#include <unistd.h>     // sleep：线程/进程休眠（秒）
#include <stdlib.h>     // malloc, free
#include <string.h>     // strcpy：字符串拷贝
#include <pthread.h>    // pthread_create, pthread_join：POSIX 线程接口

/*
 * 线程入口函数声明：
 * - 线程函数的固定签名通常为：void* func(void* arg)
 * - arg 用于接收主线程传入的参数（统一用 void* 传递）
 * - 返回值 void* 可通过 pthread_join 在主线程中取回
 */
void* thread_main(void* arg);

int main(int argc, char* argv[])
{
    pthread_t t_id;
    /*
     * pthread_t：线程标识类型
     * t_id 保存新创建线程的 ID，后续 pthread_join 需要用它来等待该线程结束
     */

    int thread_param = 5;
    /*
     * thread_param：作为线程参数传入
     * - 这里值为 5，表示线程循环打印 5 次
     *
     * 教学重点：把局部变量地址传给线程是否安全？
     * - 线程中会读取 *(int*)arg
     * - 只要主线程在 join 之前不结束，thread_param 的栈内存就仍有效
     * - 本程序会 pthread_join 等待线程结束，因此 thread_param 生命周期足够 -> 安全
     */

    void* thr_ret;
    /*
     * thr_ret：用于接收线程的返回值（void*）
     * - pthread_join 的第二个参数是一个 void**（这里传 &thr_ret）
     * - 线程返回的指针会写入 thr_ret
     */

    /*
     * pthread_create(&t_id, NULL, thread_main, (void*)&thread_param)
     *
     * 参数：
     * 1) &t_id：输出，新线程 ID
     * 2) NULL：线程属性（默认属性）
     * 3) thread_main：线程入口函数
     * 4) (void*)&thread_param：传入线程的参数指针
     *
     * 返回值：
     * - 0：成功
     * - 非 0：失败（错误码）
     */
    if(pthread_create(&t_id, NULL, thread_main, (void*)&thread_param) != 0)
    {
        puts("pthread_create() error");
        return -1;
    }

    /*
     * pthread_join(t_id, &thr_ret)
     *
     * 教学重点：join 的作用
     * 1) 阻塞主线程，直到 t_id 对应的线程结束
     * 2) 回收该线程占用的系统资源（线程的“可连接态 joinable”资源）
     * 3) 获取线程函数的返回值（若不关心可传 NULL）
     *
     * 返回值：
     * - 0：成功
     * - 非 0：失败（错误码）
     */
    if(pthread_join(t_id, &thr_ret) != 0)
    {
        puts("pthread_join() error");
        return -1;
    }

    /*
     * 线程结束后，thr_ret 中存放线程返回的指针：
     * - 在线程中返回的是 malloc 得到的 char*（字符串）
     * - 因此这里将 thr_ret 转成 (char*) 并按字符串打印
     *
     * 教学重点：为什么线程里用 malloc，而不是返回局部数组地址？
     * - 线程函数返回后，其栈帧会销毁，局部变量内存不再有效
     * - 返回局部变量地址会变成悬空指针（dangling pointer）
     * - 用 malloc 分配在堆上，生命周期可跨越线程结束，主线程拿到后再 free
     */
    printf("Thread return message: %s\n", (char*)thr_ret);

    /*
     * free(thr_ret)：释放线程分配的堆内存
     * 教学重点：
     * - 谁 malloc，谁负责 free（或至少保证最终有人 free）
     * - 这里线程 malloc、主线程 free，是一种常见的“线程返回值传递”模式
     */
    free(thr_ret);

    return 0;
}

void* thread_main(void* arg)
{
    /*
     * 将 arg 解释为 int* 并取出参数值 cnt
     * - 主线程传进来的是 &thread_param
     */
    int cnt = *((int *)arg);

    /*
     * 在堆上分配一段内存用于返回给主线程：
     * - malloc(sizeof(char) * 50)：申请 50 字节
     * - 返回值是 void*，这里赋给 char* 指针 msg
     *
     * 教学提示（不改代码，仅说明）：
     * - 实际工程中应检查 msg 是否为 NULL（内存分配失败）
     */
    char *msg = malloc(sizeof(char) * 50);

    /*
     * strcpy：把字符串拷贝进 msg 指向的缓冲区
     * - 目标缓冲区必须足够大，否则会溢出
     * - 这里分配了 50 字节，足够容纳该字符串与末尾 '\0'
     *
     * 字符串末尾包含 '\n'，所以打印时可能带换行效果
     */
    strcpy(msg, "Hello, I'm thread~\n");

    /*
     * 线程执行 cnt 次：
     * - 每次 sleep(1) 休眠 1 秒
     * - 打印 "running thread"
     *
     * 教学重点：线程与主线程的关系
     * - 主线程在 pthread_join 阻塞等待期间不会继续执行后续打印
     * - 因此会先看到线程输出 5 次，再看到主线程打印返回消息
     */
    for(int i = 0; i < cnt; i++)
    {
        sleep(1);
        puts("running thread");
    }

    /*
     * 返回线程结果：
     * - 返回的是堆内存指针 msg
     * - 主线程通过 pthread_join 得到该指针，并负责 free
     */
    return (void*)msg;
}
