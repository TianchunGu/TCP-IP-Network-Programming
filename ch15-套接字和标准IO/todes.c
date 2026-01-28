#include <stdio.h>   // 标准 I/O：printf, fputs, stderr, FILE, fdopen, fileno, fclose
#include <fcntl.h>   // open 及其标志位：O_WRONLY, O_CREAT, O_TRUNC
#include <stdlib.h>  // exit

int main(int argc, char* argv[])
{
    /*
     * open("data.dat", O_WRONLY|O_CREAT|O_TRUNC)
     *
     * 这是一个“系统调用接口”（POSIX I/O），返回的是“文件描述符”（file descriptor, FD）。
     *
     * 各标志位含义：
     * - O_WRONLY：以“只写”方式打开文件
     * - O_CREAT ：若文件不存在则创建
     * - O_TRUNC ：若文件已存在则把长度截断为 0（清空原内容）
     *
     * 教学重点：
     * - open 属于“低层 I/O”（系统调用），配套函数是 read/write/close
     * - 返回的 fd 是一个非负整数，代表内核中打开文件表的一项索引/句柄
     *
     * 注意（原理提示，不改代码）：
     * - 在标准用法中，如果使用了 O_CREAT，open 通常要传第三个参数 mode
     *   来指定新建文件权限，例如：
     *     open("data.dat", O_WRONLY|O_CREAT|O_TRUNC, 0644);
     * - 这里代码省略 mode 主要用于演示 fdopen 的概念（示例写法）
     */
    int fd = open("data.dat", O_WRONLY|O_CREAT|O_TRUNC);

    /*
     * open 失败时返回 -1
     * 常见原因：权限不足、目录不可写、路径非法等
     */
    if(fd == -1)
    {
        fputs("file open error", stderr); // 输出到标准错误，便于与正常输出区分
        exit(1);                          // 非 0 退出表示异常结束
    }

    /*
     * 打印第一次得到的“文件描述符”：
     * - 这是 open 返回的 fd
     * - 一般来说，fd 的值越小越常见（取决于进程当前占用的 FD 情况）
     */
    printf("First file descriptor: %d\n", fd);

    /*
     * fdopen(fd, "w")
     *
     * 关键教学点：把“文件描述符 fd”包装成“标准 I/O 流 FILE*”
     *
     * - fdopen 并不会重新打开文件，它只是基于现有的 fd 创建一个 FILE 对象
     * - 从此以后可以用 stdio 的接口（fputs/fgets/fprintf 等）来操作这个 fd
     *
     * "w" 的含义：
     * - 以写方式使用该流
     * - 注意：这里的模式必须与 open 时的访问方式兼容
     *   open 用的是 O_WRONLY（只写），所以 fdopen 用 "w" 是匹配的
     *
     * 标准 I/O（stdio）的重要特性：
     * - 带缓冲：写入时通常先写到用户态缓冲区，缓冲满或 fflush/fclose 才会真正写入内核
     * - 这能减少系统调用次数，提高性能
     */
    FILE* fp = fdopen(fd, "w");

    /*
     * fputs：向 fp 这个“标准 I/O 流”写入字符串
     * - 注意：这里写入的是 C 字符串，以 '\0' 结束
     * - fputs 不会自动追加换行，字符串里有 '\n' 才会换行
     *
     * 由于 stdio 可能带缓冲：
     * - 这一句执行后，数据可能仍停留在用户态缓冲区中
     * - 直到 fclose(fp) 或 fflush(fp) 才确保写入内核/文件
     */
    fputs("TCP/IP socket programming\n", fp);

    /*
     * fileno(fp)
     *
     * 教学重点：从 FILE* 中取出其“底层文件描述符 fd”
     *
     * 关键结论：
     * - fdopen 只是把同一个 fd 包装成 FILE*，并不会生成新的 fd
     * - 因此 fileno(fp) 通常会返回与最开始 open 得到的 fd 完全相同的值
     *
     * 这个例子用来验证：
     * - “First file descriptor”（open 返回的 fd）
     * - “Second file descriptor”（fileno(fp) 返回的 fd）
     * 两者应该一致
     */
    printf("Second file descriptor: %d\n", fileno(fp));

    /*
     * fclose(fp)
     *
     * 关闭标准 I/O 流：
     * - 会先刷新缓冲区（把还未写出的内容写入内核/文件）
     * - 然后释放 FILE* 相关资源
     * - 并且通常会关闭其底层文件描述符（相当于 close(fd)）
     *
     * 教学提醒：
     * - 既然 fclose 会关闭底层 fd，那么此后就不要再对 fd 调用 close
     * - 否则会出现重复关闭同一个 fd 的风险
     * - 本代码没有再 close(fd)，是符合这个规则的
     */
    fclose(fp);

    return 0; // 正常结束
}
