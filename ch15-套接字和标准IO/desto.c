#include <stdio.h>   // 标准输入输出：FILE / fputs / fclose / fdopen / stderr 等
#include <fcntl.h>   // 文件控制：open 以及 O_WRONLY / O_CREAT / O_TRUNC 等标志

int main(int argc, char* argv[])
{
    // -------------------- 使用 open() 打开/创建文件 --------------------
    // open("data.dat", O_WRONLY | O_CREAT | O_TRUNC)：
    // - "data.dat"           ：文件名
    // - O_WRONLY             ：以“只写”方式打开
    // - O_CREAT              ：如果文件不存在则创建
    // - O_TRUNC              ：如果文件已存在则将其长度截断为 0（清空原内容）
    //
    // 注意：
    // - 当使用 O_CREAT 时，严格来说 open 需要第三个参数 mode（权限位），例如 0644；
    //   本示例未提供第三个参数，属于教学/简化写法（在一些编译环境可能会产生警告或问题）。
    int fd = open("data.dat", O_WRONLY|O_CREAT|O_TRUNC);

    // open 失败返回 -1
    if(fd == -1)
    {
        // 向标准错误输出错误提示（stderr）
        fputs("file open error", stderr);
        return -1; // 以非 0 值退出表示失败
    }

    // -------------------- 用 fdopen() 将“文件描述符”转换为“标准 I/O 流” --------------------
    // fdopen(fd, "w")：
    // - fd   ：已打开的文件描述符（低层 UNIX I/O）
    // - "w"  ：以写模式创建/关联一个 FILE* 流（高层 stdio I/O）
    //
    // 这样就可以使用 fputs/fprintf 等 stdio 函数向文件写入。
    // fdopen 不会重新打开文件，而是把现有 fd “包装”为 FILE*。
    FILE* fp = fdopen(fd, "w");

    // -------------------- 通过 stdio 流写入文件 --------------------
    // fputs：把字符串写入到 fp 指向的文件流中（不自动追加 '\n'）
    // 这里字符串末尾已经包含 '\n'
    fputs("I Like ....\n", fp);

    // -------------------- 关闭文件流 --------------------
    // fclose(fp) 会：
    // 1) 刷新缓冲区（把尚未写出的数据真正写入文件）
    // 2) 关闭与该流关联的底层文件描述符 fd
    //
    // 也就是说：fclose(fp) 后，不需要再 close(fd)，否则会重复关闭。
    fclose(fp);

    return 0;
}
