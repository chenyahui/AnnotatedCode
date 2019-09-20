# libco源码分析
libco是微信开发的c/c++协程库。
之前写了一篇文章[云风coroutine协程库源码分析](http://www.cyhone.com/articles/analysis-of-cloudwu-coroutine/)，详细讲解了云风的coroutine库
在。但云风的这个库，协程的调度非常简单，主要依赖开发者手动地对协程调用`yield`和`resume`来实现协程的切换。而libco号称零侵入、无感知的。

# 协程的切换实现


# 使用epoll作为协程调度器

# hook系统函数

# 时间轮盘的实现

# cond的实现 

分三部分：
1. 协程的实现：上下文切换和共享栈
2. hook的实现
3. 为什么要有一个event_loop

libco为什么不用ucontext簇函数，而是自己实现汇编

# 参考
http://www.misakar.me/libco_share_stack/
- [微信后台 libco 协程设计及实现](https://www.infoq.cn/article/PSBYkH9dXgVeNT4Ks*l9)
- [Libco 分享](http://purecpp.org/purecpp/static/64a819e99584452aab70a7f9c307717f.pdf)