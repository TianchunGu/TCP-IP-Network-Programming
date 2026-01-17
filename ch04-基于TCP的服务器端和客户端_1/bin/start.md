# 实验一：回声服务器

服务端：

```bash
./echo_server 9190
connected client: 1
hi !
你好！
```

客户端：

```bash
./echo_client 127.0.0.1 9190
Connected......
Input Message(q/Q to quit): hi ! 
Message from server: hi !

Input Message(q/Q to quit): 你好！
Message from server: 你好！

Input Message(q/Q to quit): q
```
