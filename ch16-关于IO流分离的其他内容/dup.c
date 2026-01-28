#include <stdio.h>   // printf
#include <unistd.h>  // dup, dup2, write, close 等 POSIX 系统调用

int main(int argc, char* argv[])
{
    /*
     * 准备两段要输出的字符串：
     * 注意：这里用的是字符数组（不是 char* 指针常量），因此 sizeof(strX)
     * 会得到数组总长度，包含末尾自动补的 '\0'（字符串结束符）。
     *
     * 例如 str1 内容为 "Hi~\n"：
     * 字符序列：'H' 'i' '~' '\n' '\0'
     * 所以 sizeof(str1) 会是 5。
     * 这意味着 write 会把 '\0' 也写出去（通常显示不出来，但确实写入了字节流）。
     */
    char str1[] = "Hi~\n";
    char str2[] = "It's a nice day~~\n";

    /*
     * 文件描述符（File Descriptor, FD）基础知识（教学重点）：
     * - 在类 UNIX 系统中，一切“打开的文件/设备/管道/套接字”等都用 FD 表示
     * - 0：标准输入 stdin
     * - 1：标准输出 stdout
     * - 2：标准错误 stderr
     *
     * dup / dup2 的作用：
     * - 复制（duplicate）一个已有的 FD，得到一个新的 FD
     * - 新 FD 和旧 FD 指向同一个“打开文件描述（open file description）”
     *   也就是说它们共享：
     *   1) 文件偏移量（file offset）
     *   2) 打开状态标志（如是否追加）
     *   3) 底层打开对象（终端、文件、管道等）
     * - 但 FD 的数字值不同，相当于多了一把“门钥匙”指向同一扇门
     */

    /*
     * dup(1)
     * - 复制当前的标准输出 FD=1
     * - 返回“最小可用”的新 FD（通常是 3，因为 0/1/2 已被占用）
     * - cfd1 和 1 指向同一个输出目标（通常是终端）
     */
    int cfd1 = dup(1);

    /*
     * dup2(1, 7)
     * - 把 FD=1 复制到“指定的 FD=7”
     * - 返回值通常是 7（成功时返回 newfd）
     *
     * dup2 的规则（教学重点）：
     * 1) 如果 newfd（这里是 7）已经打开，dup2 会先把它关闭（close(7)）
     * 2) 然后让 newfd 指向 oldfd（这里是 1）所指向的同一个打开对象
     * 3) 如果 oldfd == newfd，则 dup2 什么也不做，直接返回 newfd
     */
    int cfd2 = dup2(1, 7);

    /*
     * printf 输出到 stdout（标准输出），即 FD=1
     * - printf 属于 stdio（标准I/O库），内部带缓冲
     * - 但由于这里输出有 '\n'，在终端上通常会触发“行缓冲刷新”，所以一般会立刻显示
     * - 输出内容用于观察 dup/dup2 返回的 FD 数值
     */
    printf("cfd1: %d cfd2: %d\n", cfd1, cfd2);

    /*
     * write 是系统调用，直接面向 FD 写入字节（不经过 stdio 缓冲）
     *
     * write(cfd1, str1, sizeof(str1))
     * - 用复制出来的 cfd1 写数据
     * - 因为 cfd1 指向的目标与 1 相同，所以效果等价于 write(1,...)
     */
    write(cfd1, str1, sizeof(str1));

    /*
     * write(cfd2, str2, sizeof(str2))
     * - 用指定复制到 FD=7 的 cfd2 写数据
     * - 因为 cfd2 == 7 且它也指向与 1 相同的输出目标，所以同样会输出到终端
     */
    write(cfd2, str2, sizeof(str2));

    /*
     * 关闭复制出来的 FD：
     * - close(cfd1) 只会关闭“这把钥匙”，不会影响 FD=1 仍然打开
     * - close(cfd2) 同理（关闭 FD=7），也不会影响 FD=1
     *
     * 教学重点：
     * - 多个 FD 可以指向同一个底层打开对象
     * - 关闭其中一个 FD，不会立刻关闭底层对象，除非这是最后一个引用
     */
    close(cfd1);
    close(cfd2);

    /*
     * 再次用标准输出 FD=1 写一段：
     * - 此时 FD=1 仍然是打开的，所以能正常输出
     */
    write(1, str1, sizeof(str1));

    /*
     * close(1)：关闭标准输出
     * - 从此进程的 FD=1 不再有效
     * - 这会影响所有依赖 stdout 的输出（包括 printf、puts 等）
     *
     * 教学提示：
     * - 在实际程序里一般不会随意 close(1)，这里只是为了演示 FD 的可用性与复制机制
     */
    close(1);

    /*
     * 这次 write(1, ...)：
     * - 因为 FD=1 已经被关闭，所以写入会失败
     * - write 通常返回 -1，并设置 errno = EBADF（Bad file descriptor）
     *
     * 之所以注释说“这一行应该不会被输出”，原因就是：
     * - 输出通道（FD=1）已经被关闭
     *
     * 注意：本示例代码没有检查 write 的返回值，所以你看不到错误提示，只会发现没输出
     */
    write(1, str2, sizeof(str2)); // 这一行应该不会被输出

    return 0;
}
