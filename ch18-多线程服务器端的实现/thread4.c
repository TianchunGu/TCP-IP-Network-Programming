#include <stdio.h>      // printf：输出
#include <stdlib.h>     // （本程序未用到，但保留不改动）
#include <unistd.h>     // （本程序未用到，但保留不改动）
#include <pthread.h>    // pthread_create, pthread_join：POSIX 线程接口

#define NUM_THREAD 100  // 线程总数：创建 100 个线程，一半做加法，一半做减法

/*
 * 线程函数声明：
 * - thread_inc：对全局变量 num 做 num += 1（循环很多次）
 * - thread_dec：对全局变量 num 做 num -= 1（循环很多次）
 *
 * 这段程序的教学目的通常是演示：
 * - 多线程同时读写共享变量，如果没有同步措施，会出现“竞态条件/数据竞争”
 * - 即使 long long 很大、看起来简单的 +1/-1，也并不是原子操作
 */
void* thread_inc(void* arg);
void* thread_dec(void* arg);

/*
 * 全局共享变量：
 * - num 被所有线程共享访问
 * - 理论上：50 个线程加 1 共 5,000,000 次，50 个线程减 1 共 5,000,000 次，
 *   最终应该回到 0
 *
 * 但由于并发写入没有互斥保护，实际运行中结果往往不是 0（不确定性）
 */
long long num = 0;

int main(int argc, char* argv[])
{
    pthread_t thread_id[NUM_THREAD];
    /*
     * 保存每个线程的线程 ID，便于后续 pthread_join 等待所有线程结束
     */

    /*
     * 打印 long long 的字节数（通常是 8 字节）
     * 教学意义：
     * - 有些同学会误以为“64 位变量读写就是原子的”
     * - 但即使一次读/写可能是原子的，num += 1 仍然是“读-改-写”复合操作，
     *   在并发下依然会出现丢失更新
     */
    printf("sizeof long long: %ld\n", sizeof(long long));

    /*
     * 创建 NUM_THREAD 个线程：
     * - 用 i&1 判断奇偶：
     *   * i 为奇数：创建 thread_inc（做加法）
     *   * i 为偶数：创建 thread_dec（做减法）
     *
     * 因为 NUM_THREAD=100：
     * - thread_inc 和 thread_dec 各 50 个
     *
     * 教学重点：线程调度不可预测
     * - 各线程运行顺序、交错方式由 OS 决定
     * - 这正是产生竞态条件的根源：执行交错是不可控的
     */
    for(int i = 0; i < NUM_THREAD; i++)
    {
        if(i&1)
            pthread_create(&(thread_id[i]), NULL, thread_inc, NULL);
        else
            pthread_create(&(thread_id[i]), NULL, thread_dec, NULL);
    }

    /*
     * 等待所有线程结束：
     * - pthread_join 会阻塞主线程，直到目标线程退出
     * - 确保最终打印 num 时，所有加减操作都已经完成
     */
    for(int i = 0; i < NUM_THREAD; i++)
        pthread_join(thread_id[i], NULL);

    /*
     * 输出最终结果：
     * 理论上应为 0，但常常不是 0（结果不稳定）
     *
     * 产生偏差的原因（教学重点：竞态条件）：
     * - num += 1 并不是一条原子指令，而是：
     *   1) 读取 num 到寄存器
     *   2) 寄存器 + 1
     *   3) 写回 num
     * - 如果两个线程同时执行：
     *   * 线程A读到 num=100
     *   * 线程B也读到 num=100
     *   * A 写回 101
     *   * B 写回 101
     *   结果相当于只加了一次，另一次更新“丢失”了
     *
     * 类似地，减法也会发生同样的问题
     *
     * 正确做法通常是：
     * - 用互斥锁（pthread_mutex）保护 num 的更新
     * - 或使用原子类型/原子操作（C11 atomics）
     * - 或把每个线程的结果先累积到局部变量，再汇总
     * 本题要求不改代码，这里只解释原理。
     */
    printf("Result: %lld\n", num);

    return 0;
}

void* thread_inc(void* arg)
{
    /*
     * 加法线程：循环 5,000,000 次，每次 num += 1
     *
     * 教学重点：
     * - 这里没有加锁，也没有原子操作
     * - 多线程并发执行时会发生数据竞争
     */
    for(int i = 0; i < 5000000; i++)
        num += 1;

    return NULL;
}

void* thread_dec(void* arg)
{
    /*
     * 减法线程：循环 5,000,000 次，每次 num -= 1
     * 同样没有同步机制，会与其它线程发生竞态
     */
    for(int i = 0; i < 5000000; i++)
        num -= 1;

    return NULL;
}
