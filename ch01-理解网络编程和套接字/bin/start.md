## 实验一：运行 Hello word！服务端与客户端程序

服务端`hello_server`：

```bash
./hello_server 9190 
```

客户端`hello_client`：

```bash
./hello_client 127.0.0.1 9190
Message from server: Hello World!
```
## 实验二：基于Linux的文件操作

运行 `low_open.c` 程序：此程序将创建新文件并保存数据。

```bash
./low_open 
file descriptor: 3 
```

运行 `low_read.c` ：此程序将读取文件并打印数据。

```bash
./low_read
file descriptor: 3
file data: Let's go!
```

## 实验三：基于Linux的套接字编程
运行 `fd_seri.c` ：同时创建文件和套接字，并用整数型态比较返回的文件描述符值。

```bash
./fd_seri
file descriptor 1: 3
file descriptor 2: 4
file descriptor 3: 5
```