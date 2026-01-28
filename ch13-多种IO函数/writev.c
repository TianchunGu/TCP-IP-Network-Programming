#include <stdio.h>     // 标准输入输出：printf / puts
#include <sys/uio.h>   // 分散/聚集 I/O：struct iovec / readv / writev
#include <string.h>    // 字符串操作：strlen

int main(int argc, char* agrv[])
{
    struct iovec vec[2];          // iovec 数组：描述多个缓冲区，用于 writev 的“聚集写”(gather write)
    char buf1[] = "ABCDEFG";      // 第一个待写缓冲区：字符数组形式，包含结尾 '\0'
    char buf2[] = "1234567";      // 第二个待写缓冲区：字符数组形式，同样包含结尾 '\0'
    int str_len;                  // writev 返回的实际写入总字节数

    // -------------------- 配置第一个 iovec --------------------
    // iov_base：指向要写出的数据起始地址
    // iov_len ：要写出的字节数
    //
    // 这里使用 sizeof(buf1)：
    // - buf1 是字符数组 "ABCDEFG"
    // - sizeof(buf1) 会把结尾 '\0' 也算进去
    // 因此写出的字节数为 8（A~G 共 7 字节 + '\0' 1 字节）
    vec[0].iov_base = buf1;
    vec[0].iov_len = sizeof(buf1);

    // -------------------- 配置第二个 iovec --------------------
    // 这里使用 strlen(buf2)：
    // - strlen 不包含结尾 '\0'
    // 因此写出的字节数为 7（只包含 '1'~'7'）
    //
    // 对比可见：buf1 会写出 '\0'，buf2 不会写出 '\0'
    // 写到终端时，'\0' 通常不可见，但它确实占用 1 个字节计数
    vec[1].iov_base = buf2;
    vec[1].iov_len = strlen(buf2);

    // -------------------- 聚集写：一次系统调用写出多个缓冲区 --------------------
    // writev(1, vec, 2)：
    // - 第一个参数 1 表示标准输出 stdout（终端）
    // - vec 指向 iovec 数组，包含 2 段要写出的数据
    // - 2 表示 iovec 的数量
    //
    // writev 的行为（简化理解）：
    // - 按顺序把 vec[0] 的 iov_len 字节写出，再写 vec[1] 的 iov_len 字节
    // - 尽量在一次系统调用中完成，减少多次 write 的开销
    // 返回值：
    // - 成功：返回实际写出的总字节数（可能小于请求写出的总长度，取决于 fd 状态）
    // - 失败：返回 -1（本示例未做错误处理）
    str_len = writev(1, vec, 2);

    // puts("")：输出一个换行（puts 会自动在字符串末尾输出 '\n'）
    // 这里相当于把光标换到下一行，便于后面的 printf 输出更整齐
    puts("");

    // 输出 writev 实际写入的字节数
    // 注意：由于 vec[0] 使用 sizeof(buf1) 包含 '\0'，所以计数会比“可见字符”多 1
    printf("Write %d bytes\n", str_len);

    return 0;
}
