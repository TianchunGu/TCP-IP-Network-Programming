#include <stdio.h>      // 标准 I/O：FILE、fopen、fgets、fputs、fclose 等

#define BUF_SIZE 3      // 缓冲区大小为 3（非常小，用来演示 fgets 的行为）
                        // 注意：fgets 最多读取 (BUF_SIZE - 1) 个字符，
                        //       剩下 1 个位置必须留给字符串结束符 '\0'。
                        // 因此这里每次最多只能读 2 个字符 + '\0'。

int main(int argc, char* argv[])
{
    FILE* fp1, *fp2;    // fp1：源文件指针（读），fp2：目标文件指针（写）
    char buf[BUF_SIZE]; // 读取缓冲区（长度 3）
                        // buf 的实际有效内容会是一个 C 字符串：
                        // 例如：{'H','i','\0'} 或 {'\n','\0',?}（? 表示未被覆盖的旧值）
                        // 关键：fgets 会保证读到的字符串以 '\0' 结束（除非出错或 EOF 且无内容）。

    /*
     * fopen("news.txt", "r")：
     * - 打开名为 news.txt 的文件，模式 "r" 表示只读
     * - 成功返回 FILE*；失败返回 NULL（例如文件不存在或权限不足）
     *
     * 教学提示：示例代码未做 NULL 检查。
     * 实际工程中通常需要判断 fp1/fp2 是否为 NULL，否则可能崩溃。
     */
    fp1 = fopen("news.txt", "r");

    /*
     * fopen("cpy.txt", "w")：
     * - 打开/创建名为 cpy.txt 的文件，模式 "w" 表示只写
     * - 若文件已存在，会先清空（截断为 0 字节）再写入
     * - 成功返回 FILE*；失败返回 NULL
     */
    fp2 = fopen("cpy.txt", "w");

    /*
     * while (fgets(buf, BUF_SIZE, fp1) != NULL)
     *
     * fgets 的读取规则（教学重点）：
     * 1) 最多读取 BUF_SIZE-1 个字符（这里是 2 个字符）；
     * 2) 如果在读满前遇到换行符 '\n'，会把 '\n' 也读入 buf；
     * 3) 读完后自动追加 '\0'，保证 buf 是以 '\0' 结尾的字符串；
     * 4) 返回值：
     *    - 成功读取到至少 1 个字符：返回 buf 的地址（非 NULL）
     *    - 到达 EOF 且没有读到任何字符 / 或发生错误：返回 NULL
     *
     * 因为 BUF_SIZE=3，每次最多读 2 个字符，所以会出现“分段读取”的现象：
     * 假设 news.txt 某行是： "HELLO\n"
     * - 第 1 次 fgets：读入 "HE\0"
     * - 第 2 次 fgets：读入 "LL\0"
     * - 第 3 次 fgets：读入 "O\n\0"（因为遇到 '\n'，会把换行也读进来）
     *
     * 这说明：即使一行很长，fgets 也不会一次读完，而会按缓冲区大小多次读取；
     * 但它仍然“按行优先”：遇到 '\n' 会停止，并把 '\n' 带回给你。
     */
    while(fgets(buf, BUF_SIZE, fp1) != NULL)
        /*
         * fputs(buf, fp2)：
         * - 将 buf 指向的 C 字符串写入到 fp2（目标文件）
         * - fputs 不会自动加换行；写多少取决于 buf 里有多长（直到 '\0'）
         *
         * 结合上面的分段读取：
         * - 由于每次读入的只是“原文件的一小段”，这里也会一小段一小段写入；
         * - 最终效果等价于把源文件完整复制到目标文件（包括换行符也会被复制过去）。
         */
        fputs(buf, fp2);

    /*
     * fclose(fp1) / fclose(fp2)：
     * - 关闭文件流，释放资源
     * - 对写入流来说，fclose 还会触发缓冲区刷新（把还没真正写到磁盘的数据写出去）
     */
    fclose(fp1);
    fclose(fp2);

    return 0; // 程序正常结束
}
