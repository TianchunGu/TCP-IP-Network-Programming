# 实验一

该示例验证TCP套接字具有以下特性：传输的数据不存在边界。为验证这一点，需要让 `write` 函数的调用次数不同于 `read` 函数的调用次数。因此在客户端中分多次调用 `read` 函数以接收服务器端发送的全部数据。tcp_server.c 与第一章的hello_server.c相比无变化。

服务端：

```bash
./tcp_server 9190
```

客户端：

```bash
./tcp_client 127.0.0.1 9190
Message from server: Hello World

Function read call count: 13
```
