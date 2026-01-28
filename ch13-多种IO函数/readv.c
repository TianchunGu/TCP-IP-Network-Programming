#include <stdio.h>     // 标准输入输出：printf
#include <sys/uio.h>   // 分散/聚集 I/O：struct iovec / readv / writev
#define BUF_SIZE 100   // 每个缓冲区最大长度（这里 buf1、buf2 都是 100 字节）

int main(int argc, char* argv[])
{
    struct iovec vec[2];            // iovec 数组：描述多个缓冲区，用于 readv 的“分散读”(scatter read)
    char buf1[BUF_SIZE] = {0, };    // 第一个缓冲区：初始化为全 0，便于当作字符串输出（含 '\0'）
    char buf2[BUF_SIZE] = {0, };    // 第二个缓冲区：同样初始化为全 0
    int str_len;                    // readv 返回的实际读取总字节数

    // -------------------- 配置第一个 iovec --------------------
    // iov_base：指向目标缓冲区的起始地址
    // iov_len ：该缓冲区可接收的最大字节数
    //
    // 这里设置 buf1 只接收 5 个字节：
    // 也就是说 readv 会优先把输入数据填满 buf1 的前 5 字节
    vec[0].iov_base = buf1;
    vec[0].iov_len = 5;

    // -------------------- 配置第二个 iovec --------------------
    // 第二个缓冲区 buf2 可接收 BUF_SIZE(100) 字节
    // 当 buf1 被填满（或输入结束）后，剩余数据会继续写入 buf2
    vec[1].iov_base = buf2;
    vec[1].iov_len = BUF_SIZE;

    // -------------------- 分散读：从文件描述符 0 读取 --------------------
    // readv(0, vec, 2)：
    // - 第一个参数 0 表示标准输入 stdin（键盘输入）
    // - vec 指向 iovec 数组，包含 2 个缓冲区描述
    // - 2 表示 iovec 的数量
    //
    // readv 的行为（简化理解）：
    // 1) 先向 vec[0] 指向的 buf1 写入最多 5 字节
    // 2) 如果还有剩余输入，再向 vec[1] 指向的 buf2 写入最多 100 字节
    // 3) 返回值为“总共读到的字节数”（两段缓冲区之和）
    //
    // 注意：
    // - readv 不会自动添加字符串结束符 '\0'；
    // - 但这里 buf1、buf2 初始全为 0，因此若读入的字节数未覆盖到末尾，
    //   仍可能在后面保留 '\0'，从而 printf("%s") 看起来能正常输出（教学示例常用技巧）。
    // - 若刚好读满某段缓冲区且输入中没有 '\0'，直接用 %s 输出可能出现越界读取风险。
    str_len = readv(0, vec, 2);

    // 输出 readv 读到的总字节数
    printf("Read %d Bytes\n", str_len);

    // 输出第一段缓冲区内容（前 5 字节来自输入，其后仍然是初始化的 0）
    printf("First message: %s\n", buf1);

    // 输出第二段缓冲区内容（输入剩余部分）
    printf("Second message %s\n", buf2);

    return 0;
}
