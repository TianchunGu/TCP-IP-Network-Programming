#include <stdio.h>       // 标准输入输出：puts / printf
#include <unistd.h>      // POSIX：read（从文件描述符读取数据）
#include <sys/time.h>    // 时间结构：struct timeval
#include <sys/select.h>  // I/O 多路复用：select / fd_set / FD_* 宏

// 程序功能：监视标准输入（控制台），如果在 5 秒内有输入就打印出来，否则提示超时。

#define BUF_SIZE 30      // 从控制台读取数据的缓冲区大小（最多读取 30 字节）

int main(int argc, char* argv[])
{
    fd_set reads, temps;         // reads：关注的读事件集合；temps：select 调用时使用的临时集合
    int result, str_len;         // result：select 返回值；str_len：read 实际读取字节数
    char buf[BUF_SIZE];          // 读取数据的缓冲区
    struct timeval timeout;      // select 的超时参数（秒 + 微秒）

    // -------------------- 初始化监听集合 reads --------------------
    // FD_ZERO：把 fd_set 清空（所有位清 0），表示当前集合里没有任何文件描述符
    FD_ZERO(&reads);

    // FD_SET(0, &reads)：把文件描述符 0 加入 reads 集合
    // 文件描述符 0 通常是标准输入 stdin（键盘输入）
    // 这里的目标是：监视“标准输入是否可读”
    FD_SET(0, &reads);

    // 循环等待用户输入或超时
    while(1)
    {
        // -------------------- select 会修改 fd_set，因此要用副本 --------------------
        // select 调用返回后，会把“就绪”的 fd 保留在集合中，未就绪的清掉；
        // 若下一次还想继续监视同一批 fd，必须在每次调用前重新设置。
        // 因此这里把 reads 复制到 temps，让 reads 作为“原始模板”保留。
        temps = reads;

        // -------------------- 设置超时时间 --------------------
        // tv_sec：秒
        // tv_usec：微秒（1 秒 = 1,000,000 微秒）
        // 这里表示：最多等待 5 秒
        timeout.tv_sec = 5;
        timeout.tv_usec = 0;

        // -------------------- 调用 select 进行 I/O 多路复用等待 --------------------
        // select 的原型（简化理解）：
        // int select(int nfds, fd_set *readfds, fd_set *writefds,
        //            fd_set *exceptfds, struct timeval *timeout);
        //
        // 参数解释：
        // 1) nfds：
        //    需要监视的“最大文件描述符值 + 1”
        //    这里我们只监视 fd=0，因此 nfds=0+1=1
        // 2) &temps：
        //    读事件集合：返回时会被修改，只保留就绪的 fd
        // 3) 0：
        //    不监视写事件
        // 4) 0：
        //    不监视异常事件
        // 5) &timeout：
        //    超时等待时间；注意：select 返回后 timeout 也可能被修改为“剩余时间”，
        //    所以每次循环都重新赋值是必要的
        result = select(1, &temps, 0, 0, &timeout);

        // -------------------- 处理 select 返回值 --------------------
        // result 的语义：
        // - -1：出错（例如被信号中断等）
        // -  0：超时（在 timeout 时间内没有任何 fd 就绪）
        // - >0：就绪的 fd 数量（readfds/writefds/exceptfds 中就绪的总数）
        if(result == -1)
        {
            puts("select() error!");
            break; // 出错则跳出循环
        }
        else if(result == 0)
        {
            // 超时：5 秒内没有输入可读
            puts("Time-out!");
        }
        else
        {
            // 有至少一个 fd 就绪（本例只监视 fd=0）
            // FD_ISSET(0, &temps)：判断 fd=0 是否在“就绪集合”中
            if(FD_ISSET(0, &temps))
            {
                // 从标准输入读取数据（最多 BUF_SIZE 字节）
                // read 返回值 str_len：
                // - >0：成功读取到的字节数
                // - 0 ：EOF（例如输入流被关闭）
                // - -1：出错（本例未单独处理）
                str_len = read(0, buf, BUF_SIZE);

                // 将读取到的数据当作 C 字符串输出时，需要手动添加 '\0'
                // 注意：如果 str_len == BUF_SIZE，则 buf[str_len] 会越界；
                // 更稳妥的写法通常是 read(0, buf, BUF_SIZE-1) 预留 '\0' 空间。
                buf[str_len] = 0;

                // 输出用户输入内容
                printf("Message from console: %s\n", buf);
            }
        }
    }

    return 0;
}
