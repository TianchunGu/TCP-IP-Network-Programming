#include <stdio.h>   // 这里其实没有用到 stdio 的函数（如 printf），但保留不改动
#include <fcntl.h>   // open() 需要的标志位：O_RDONLY / O_WRONLY / O_CREAT / O_TRUNC 等
#include <unistd.h>  // read() / write() / close() 等 POSIX 系统调用

#define BUF_SIZE 3 // 缓冲区大小为 3（刻意设置得很小，用来演示“分块读取/写入”）
                   // 注意：与 fgets 不同，read 是“按字节流”读取，不关心行与 '\n'

int main(int argc, char* argv[])
{
    int fd1, fd2, len;
    /*
     * fd1：源文件（news.txt）的文件描述符（file descriptor）
     * fd2：目标文件（cpy.txt）的文件描述符
     * len：每次 read 实际读取到的字节数
     *
     * 文件描述符（FD）是操作系统内核维护的一个整数“句柄”：
     * - 0：标准输入 stdin
     * - 1：标准输出 stdout
     * - 2：标准错误 stderr
     * - open 成功后会返回一个 >= 0 的整数作为 FD
     */

    char buf[BUF_SIZE];
    /*
     * buf：原始字节缓冲区（不是“字符串缓冲区”）
     * 教学重点：
     * - read() 读出来的是“字节”，不会自动添加 '\0'
     * - buf 内的数据不一定是可打印字符，也不保证以 '\0' 结尾
     * - 这里我们只是把读到的字节原样写出去，不需要把它当字符串
     */

    /*
     * open("news.txt", O_RDONLY)
     * - 打开文件 news.txt
     * - O_RDONLY：只读方式
     *
     * 返回值：
     * - 成功：返回一个非负整数 FD
     * - 失败：返回 -1（例如文件不存在、权限不足等）
     *
     * 教学提示：示例代码未检查 open 是否返回 -1。
     * 真实工程中应检查 fd1/fd2 是否有效，否则 read/write 可能失败。
     */
    fd1 = open("news.txt", O_RDONLY);

    /*
     * open("cpy.txt", O_WRONLY | O_CREAT | O_TRUNC)
     *
     * 标志位含义（可组合，用按位或 | 拼在一起）：
     * - O_WRONLY：只写打开
     * - O_CREAT ：文件不存在则创建
     * - O_TRUNC ：如果文件已存在，则把文件长度截断为 0（清空原内容）
     *
     * 重要原理提示：
     * - 当使用 O_CREAT 时，open 的标准形式通常是 open(path, flags, mode)
     *   也就是需要第三个参数 mode（例如 0644）来指定新建文件的权限。
     * - 本代码只有两个参数，这是示例写法；在严格/标准用法中可能会产生问题或警告，
     *   但题目要求不改代码，这里只做原理说明。
     */
    fd2 = open("cpy.txt", O_WRONLY|O_CREAT|O_TRUNC);

    /*
     * while ((len = read(fd1, buf, sizeof(buf))) > 0)
     *
     * read(fd, buf, count) 的含义：
     * - 从文件描述符 fd 指向的“文件当前位置”读取最多 count 个字节到 buf
     * - 返回值是“实际读取到的字节数”
     *
     * 返回值规律（教学重点）：
     * - len > 0：成功读取到 len 个字节（len 可能小于 count）
     * - len == 0：到达文件末尾 EOF（没有更多数据可读）
     * - len == -1：发生错误（例如 fd 无效、权限问题、被信号中断等）
     *
     * 为什么 len 可能小于 sizeof(buf)？
     * - 文件剩余字节不足 count 时，read 会只返回剩余字节数
     * - 某些情况下（例如读取管道/终端/网络）read 也可能“提前返回”更少字节
     *
     * 因为 BUF_SIZE=3，所以这里会以“每次最多 3 字节”的方式分块复制文件：
     * - 第 1 次读 3 字节 -> 写 3 字节
     * - 第 2 次读 3 字节 -> 写 3 字节
     * - ...
     * - 最后一次可能读到 1~2 字节（取决于文件长度是否为 3 的倍数）
     */
    while((len = read(fd1, buf, sizeof(buf))) > 0)
        /*
         * write(fd2, buf, len)
         * - 向目标文件 fd2 写入 len 个字节（注意不是 sizeof(buf)，而是“实际读到多少写多少”）
         *
         * 关键原因：
         * - 最后一次 read 可能只读到 1 或 2 字节，如果仍写 sizeof(buf)=3，
         *   就会把 buf 中旧数据或未定义内容也写进文件，导致复制结果错误
         *
         * 教学补充：
         * - write 的返回值是“实际写入的字节数”，也可能小于请求写入的 count
         *   （对普通文件通常会写满，但对管道/网络/非阻塞 I/O 可能发生部分写）
         * - 示例代码没有检查 write 的返回值，工程中一般要处理“部分写入”的情况
         */
        write(fd2, buf, len);

    /*
     * close(fd)：关闭文件描述符，释放内核资源
     * - 关闭后该 FD 不可再用
     * - 对写文件而言，close 会促使内核把数据刷新到磁盘（但具体刷盘时机仍由系统决定）
     */
    close(fd1);
    close(fd2);

    return 0; // 程序正常结束
}
