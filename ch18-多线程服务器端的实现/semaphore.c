#include <stdio.h>      // printf, fputs, scanf, stdout
#include <pthread.h>    // pthread_create, pthread_join：线程创建与等待
#include <semaphore.h>  // sem_t, sem_init, sem_wait, sem_post, sem_destroy：POSIX 信号量

/*
 * 线程函数声明：
 * - read：负责从键盘读取 5 个整数，依次写入共享变量 num
 * - accu：负责读取共享变量 num 的值并累计求和，最终输出结果
 *
 * 这段程序的核心教学点是：
 * - 用两个信号量实现两个线程之间的“严格交替执行”（同步/握手）
 * - 让 read 线程每次输入一个数，accu 线程就处理一次，避免读写冲突
 */
void* read(void* arg);
void* accu(void* arg);

/*
 * 两个信号量（semaphore）：
 *
 * sem_one：用于通知 accu 线程“新数据已准备好，可以读 num 了”
 * sem_two：用于通知 read 线程“accu 已经处理完当前数据，可以再输入下一个了”
 *
 * 它们构成一种“乒乓/握手协议”：
 * read 线程 -> (post sem_one) -> accu 线程
 * accu 线程 -> (post sem_two) -> read 线程
 */
static sem_t sem_one;
static sem_t sem_two;

/*
 * 共享变量 num：
 * - read 线程写入（scanf 读到的数字放入 num）
 * - accu 线程读取（把 num 加到 sum）
 *
 * 教学重点：
 * - num 是共享资源，如果没有同步机制，两个线程可能同时访问导致竞态
 * - 这里通过信号量保证：同一时刻只有一个线程在访问 num（严格交替）
 */
static int num;

int main(int argc, char* argv[])
{
    pthread_t id_t1, id_t2;

    /*
     * sem_init(&sem_one, 0, 0)
     * - 初始化信号量 sem_one
     * - 第二个参数 pshared=0：表示线程间共享（同一进程内），不是进程间共享
     * - 初值=0：表示 accu 线程一开始“拿不到许可”，会先阻塞等待
     *
     * 含义：
     * - 程序开始时还没有输入任何数字，因此 accu 不应该先运行累加逻辑
     */
    sem_init(&sem_one, 0, 0);

    /*
     * sem_init(&sem_two, 0, 1)
     * - 初始化信号量 sem_two，初值=1
     *
     * 含义：
     * - read 线程一开始应该允许进行第一次输入
     * - 因此给它 1 个“许可”，read 第一次 sem_wait(&sem_two) 可以立即通过
     */
    sem_init(&sem_two, 0, 1);

    /*
     * 创建两个线程：
     * - id_t1 执行 read：输入线程
     * - id_t2 执行 accu：累加线程
     *
     * 教学重点：
     * - 两线程并发启动，但通过信号量控制它们的执行先后顺序
     */
    pthread_create(&id_t1, NULL, read, NULL);
    pthread_create(&id_t2, NULL, accu, NULL);

    /*
     * 等待两个线程结束：
     * - 主线程在这里阻塞，直到 read 和 accu 都执行完
     * - 保证输出结果发生在两个线程完成之后
     */
    pthread_join(id_t1, NULL);
    pthread_join(id_t2, NULL);

    /*
     * 销毁信号量，释放资源
     */
    sem_destroy(&sem_one);
    sem_destroy(&sem_two);

    return 0;
}

void* read(void* arg)
{
    /*
     * read 线程：循环 5 次读取输入
     * 每次流程（教学重点）：
     * 1) sem_wait(sem_two)：等待“允许输入”的许可
     * 2) scanf 把输入写入共享变量 num
     * 3) sem_post(sem_one)：通知 accu 线程“新数据已就绪，可以读取 num”
     *
     * 这样就保证：
     * - accu 不会在 num 未更新时读取
     * - read 不会在 accu 未处理完当前 num 时覆盖 num
     */
    for(int i = 0; i < 5; i++)
    {
        /*
         * 提示用户输入
         * fputs 输出到 stdout（标准输出）
         */
        fputs("Input num: ", stdout);

        /*
         * sem_wait(&sem_two)：
         * - “P 操作 / wait 操作”
         * - 若 sem_two 的值 > 0，则减 1 并立即返回（获得许可）
         * - 若 sem_two 的值 == 0，则阻塞等待，直到其他线程 sem_post 增加它
         *
         * 因为 sem_two 初值为 1，所以第一次不会阻塞；
         * 后续是否阻塞取决于 accu 线程是否已经处理完并 post sem_two。
         */
        sem_wait(&sem_two);

        /*
         * scanf("%d", &num)：
         * - 从标准输入读取一个整数，写入共享变量 num
         *
         * 教学提示：
         * - scanf 是阻塞式输入，会等待用户键入并回车
         * - 在 sem_two 的保护下，确保同一时刻 accu 不会读取 num
         */
        scanf("%d", &num);

        /*
         * sem_post(&sem_one)：
         * - “V 操作 / post 操作”
         * - 将 sem_one 的值加 1
         * - 如果有线程正在 sem_wait(sem_one) 阻塞等待，则会唤醒其中一个
         *
         * 含义：
         * - 告诉 accu 线程：num 已经填好，可以读取并累加
         */
        sem_post(&sem_one);
    }

    return NULL;
}

void* accu(void* arg)
{
    /*
     * accu 线程：循环 5 次累加
     * 每次流程：
     * 1) sem_wait(sem_one)：等待“新数据可用”
     * 2) sum += num：读取共享变量 num 并累加
     * 3) sem_post(sem_two)：通知 read 线程“我已处理完，你可以输入下一个”
     *
     * 教学重点：
     * - sem_one 初值是 0，所以 accu 一开始会阻塞，直到 read 输入第一个数并 post sem_one
     * - 这实现了严格的先输入后处理的顺序
     */
    int sum = 0;

    for(int i = 0; i < 5; i++)
    {
        /*
         * 等待 read 线程输入完一个数：
         * - 没有新数据时会阻塞
         */
        sem_wait(&sem_one);

        /*
         * 此时 read 线程已经把新值写入 num，且不会同时写（因为它在等待 sem_two）
         * 因而这里读取 num 是安全的
         */
        sum += num;

        /*
         * 通知 read 线程可以进行下一次输入：
         * - sem_two +1，让 read 的 sem_wait(&sem_two) 得以继续
         */
        sem_post(&sem_two);
    }

    /*
     * 循环结束后输出累加结果
     */
    printf("Result: %d\n", sum);

    return NULL;
}
