#include <stdio.h>      // printf：输出结果
#include <pthread.h>    // pthread_create, pthread_join：POSIX 线程接口

/*
 * 线程函数声明：
 * thread_summation：对一个整数区间求和，并把结果累加到全局变量 sum
 */
void* thread_summation(void* arg);

/*
 * 全局共享变量 sum：
 * - 两个线程都会对它进行读写（sum += start）
 * - 教学重点：这会引入“竞态条件（race condition）”
 *   因为 sum += start 并不是原子操作，底层至少包含：
 *   1) 从内存读出 sum
 *   2) 与 start 相加
 *   3) 写回 sum
 *   两个线程交错执行时可能丢失更新，导致最终结果不稳定
 *
 * 这段代码的价值通常在于演示：不加同步的共享变量会产生数据竞争。
 */
int sum = 0;

int main(int argc, char* argv[])
{
    pthread_t id_t1, id_t2;
    /*
     * id_t1 / id_t2：保存两个线程的线程ID，用于 join 等待它们结束
     */

    int range1[] = {1, 5};
    int range2[] = {6, 10};
    /*
     * range1 和 range2：分别表示两个线程要处理的求和区间
     * - range1 = [1, 5]
     * - range2 = [6, 10]
     *
     * 教学重点：这里把“数组首地址”作为参数传给线程
     * - 在线程函数中会把 arg 转成 int*，取下标 0 和 1 作为 start/end
     * - range1/range2 是 main 的局部变量，但 main 会一直等待 join，
     *   在 join 结束前 main 不会返回，因此它们在栈上的内存仍然有效 -> 这里是安全的
     */

    /*
     * 创建线程 1：处理 [1, 5]
     * pthread_create 第四个参数是 void*，这里传 range1 的首地址
     */
    pthread_create(&id_t1, NULL, thread_summation, (void*)range1);

    /*
     * 创建线程 2：处理 [6, 10]
     */
    pthread_create(&id_t2, NULL, thread_summation, (void*)range2);

    /*
     * 等待两个线程结束：
     * - pthread_join 会阻塞主线程，直到对应线程退出
     * - 确保打印 sum 时两个线程都已经完成累加
     */
    pthread_join(id_t1, NULL);
    pthread_join(id_t2, NULL);

    /*
     * 输出最终结果：
     * 理论上 1~10 的和应为 55
     * 但由于 sum 是共享变量且没有互斥保护，
     * 实际运行中可能出现非 55 的结果（尤其在多核/高并发情况下更明显）。
     */
    printf("Result: %d\n", sum);
    return 0;
}

void* thread_summation(void* arg)
{
    /*
     * 解析线程参数：
     * arg 指向一个 int 数组，约定：
     * - [0] 为起始值 start
     * - [1] 为结束值 end
     *
     * ((int*)arg)[0]：先把 arg 转为 int*，再取第 0 个元素
     */
    int start = ((int*)arg)[0];
    int end = ((int*)arg)[1];

    /*
     * 对区间 [start, end] 做累加：
     * - 每轮把 start 加到全局 sum 上
     * - 然后 start++
     *
     * 教学重点：sum += start 的并发问题
     * - 两个线程可能同时读取旧的 sum，并在各自加法后写回，
     *   导致其中一个线程的更新被覆盖（“丢失更新”）
     * - 正确做法通常是：
     *   1) 用互斥锁保护 sum（pthread_mutex）
     *   2) 或让每个线程先在局部变量中求和，最后再合并（减少锁开销）
     *   3) 或使用原子操作（atomic）等
     * 但题目要求不改代码，这里只解释原理。
     */
    while(start <= end)
    {
        sum += start;
        start++;
    }

    return NULL; // 线程结束，无返回值
}
