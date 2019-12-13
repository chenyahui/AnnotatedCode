# libevent的epoll优化
epoll是Linux下面IO事件通知的API。
而libevent如何使用epoll呢？
libevent是一个跨平台的事件库，因此用了统计的接口表示了

epoll说到底只有三个api：

# epoll_create
epoll_create的参数取多少？

```c
epoll_create(32000)
```

新的API
```c
epoll_create1(EPOLL_CLOEXEC);
```
create1和create的区别在哪里？

timerfd

changelist的危害

# 

# epoll 
# changelist机制

实际上epoll并没有说是可以监听某个特别的事件，它只对某个fd进行事件监控。实际上是可以的，在epoll_ctl
# 参考
[从源码角度讲epoll与select的区别](https://www.cnblogs.com/apprentice89/p/3234677.html)