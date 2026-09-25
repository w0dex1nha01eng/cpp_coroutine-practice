# C++20 Coroutine

## what's this

最近在看这个C++20 协程，学着抄着写着然后还得问着AI

## 项目结构

- `include/task.hpp`：最简协程任务DetachedTask
- `include/event_loop.hpp` + `src/event_loop.cpp`：poll事件循环
- `include/socket.hpp` + `src/socket.cpp`：非阻塞socket和四个awaiter
- `achieve/echo_server.cpp`：echo服务端
- `achieve/echo_client.cpp`：echo客户端
- `include/compat/posix_sockets.hpp`：Windows上缺的POSIX接口补丁（最开始在mac上clion构筑的，回到win上一片红让ai帮忙改的）

## 构建

需要支持C++20的编译器（GCC 10+ / Clang 12+）和CMake 3.20+

Linux/macOS：

```bash
cmake -S . -B build
cmake --build build
```

Windows（MinGW-w64）：

```powershell
cmake -S . -B build -G "MinGW Makefiles"
cmake --build build
```

## 运行

先开一个终端跑server：

```bash
./build/echo_server 9000
```

再开一个终端跑client：

```bash
./build/echo_client 127.0.0.1 9000 "hello"
```

## 已知问题

1. 当前默认一个fd对应的还是单线程
2. 没有超时检测
3. 没有cancellation
4. 客户端返回超过4096字节的话剩下的数据会扔掉只返回4096
