#include <stdio.h>      // printf
#include <unistd.h>     // （本程序未用到，但保留不改动；常见于示例代码中）
#include <stdlib.h>     // （本程序未用到，但保留不改动）
#include <pthread.h>    // pthread_create, pthread_join, pthread_mutex_* 等线程/互斥锁接口

#define NUM_THREAD 100  // 线程总数：创建 100 个线程，其中一半做加操作，一半做减操作

void *thread_dec(void *arg); // 线程函数：对全局变量 num 做“加 1”操作（名字 dec 但实际是加）
void *thread_inc(void *arg); // 线程函数：对全局变量 num 做“减 1”操作（名字 inc 但实际是减）

/*
 * 全局共享变量：
 * num：所有线程都会读写它，因此存在“共享数据竞争”（data race）风险
 *
 * long long 用于保存较大的结果：
 * - 每个线程循环 5,000,000 次
 * - 50 个线程做加、50 个线程做减，理论上最终应回到 0
 */
long long num = 0;

/*
 * 互斥锁（mutex）：
 * - 用于保护对 num 的访问，确保同一时刻只有一个线程能修改 num
 * - 这是解决“竞态条件（race condition）”最典型的方法之一
 */
pthread_mutex_t mutex;

int main(int argc, char *argv[])
{
    pthread_t thread_id[NUM_THREAD];
    /*
     * pthread_t：线程标识类型
     * thread_id 数组用于保存每个线程的 ID，便于后续 pthread_join 等待线程结束
     */

    /*
     * 初始化互斥锁：
     * - mutex 初始状态为未加锁
     * - 第二个参数为 NULL 表示使用默认属性
     */
    pthread_mutex_init(&mutex, NULL);

    /*
     * 创建 NUM_THREAD 个线程：
     * - i & 1 用于判断 i 的奇偶性：
     *   * i 为奇数：创建 thread_dec 线程
     *   * i 为偶数：创建 thread_inc 线程
     *
     * 因为 NUM_THREAD=100：
     * - 将创建 50 个 thread_dec（加 1）线程
     * - 将创建 50 个 thread_inc（减 1）线程
     *
     * 教学重点：线程创建后是并发运行的
     * - 操作系统会调度它们交替执行（顺序不可预测）
     */
    for (int i = 0; i < NUM_THREAD; i++)
    {
        if (i & 1)
            pthread_create(&(thread_id[i]), NULL, thread_dec, NULL);
        else
            pthread_create(&(thread_id[i]), NULL, thread_inc, NULL);
    }

    /*
     * 等待所有线程结束：
     * pthread_join 会阻塞当前线程（main 线程），直到目标线程运行结束
     *
     * 教学重点：
     * - 如果不 join，main 可能先结束退出，导致子线程还没执行完程序就退出了
     * - join 还能确保最终打印的 num 是所有线程完成后的最终结果
     */
    for (int i = 0; i < NUM_THREAD; i++)
        pthread_join(thread_id[i], NULL);

    /*
     * 理论结果分析（教学重点）：
     * - thread_dec：每个线程把 num 加 1 共 5,000,000 次
     * - thread_inc：每个线程把 num 减 1 共 5,000,000 次
     * - 由于各 50 个线程数量相同，若每次加减都严格正确完成，
     *   最终结果应为 0
     *
     * 但是关键点在于：如果没有互斥锁保护，
     * num += 1 / num -= 1 不是原子操作，可能发生竞态，最终结果会偏离 0。
     * 本程序通过互斥锁保证“修改 num 的临界区”互斥执行，因此能得到稳定结果。
     */
    printf("Result: %lld\n", num);

    /*
     * 销毁互斥锁：
     * - 释放互斥锁相关资源
     * - 一般在不再需要该锁时调用
     */
    pthread_mutex_destroy(&mutex);

    return 0;
}

void *thread_dec(void *arg)
{
    /*
     * thread_dec 线程函数（名字叫 dec，但实际执行的是 num += 1）
     *
     * 教学重点：互斥锁保护“临界区”
     * - 临界区：对共享变量 num 的读-改-写过程
     * - 如果多个线程同时进入临界区，会发生数据竞争
     *
     * pthread_mutex_lock：
     * - 请求加锁
     * - 若锁已被其他线程持有，本线程会阻塞等待，直到锁可用
     */
    pthread_mutex_lock(&mutex);

    /*
     * 在锁保护下对 num 做大量修改：
     * - 循环 5,000,000 次，每次把 num 加 1
     *
     * 教学提示（非常重要）：
     * - 由于锁是在整个循环外层加的，
     *   这意味着：一个线程会独占锁把 5,000,000 次都做完，其他线程必须一直等待
     * - 结果是：正确性有保证，但并发性能会很差（几乎变成串行）
     *
     * 若想提高并发性，通常会把锁粒度变小（例如每次加减都锁/解锁，或分批累加后再合并）
     * 但本题要求不改代码，这里只解释现象。
     */
    for (int i = 0; i < 5000000; i++)
        num += 1;

    /*
     * pthread_mutex_unlock：
     * - 释放锁
     * - 让其他等待该锁的线程有机会进入临界区
     */
    pthread_mutex_unlock(&mutex);

    return NULL;
}

void *thread_inc(void *arg)
{
    /*
     * thread_inc 线程函数（名字叫 inc，但实际执行的是 num -= 1）
     * 与 thread_dec 对称，同样用互斥锁保护共享变量 num
     */
    pthread_mutex_lock(&mutex);

    /*
     * 循环 5,000,000 次，每次把 num 减 1
     * 同样由于锁粒度很大，会导致线程基本串行执行
     */
    for (int i = 0; i < 5000000; i++)
        num -= 1;

    pthread_mutex_unlock(&mutex);

    return NULL;
}
